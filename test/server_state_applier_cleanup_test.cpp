/*
 * Copyright (C) 2026 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

//
// MDEV-38843 regression tests.
//
// A number of static helpers in src/server_state.cpp finish an applier
// write set with the idiom:
//
//     ret = ret || <some operation that may fail>;
//     ret = ret || (high_priority_service.after_apply(), 0);
//
// Because of the short-circuiting "||", after_apply() is skipped whenever
// the preceding operation already failed. Skipping after_apply() leaves
// the applier transaction "active", which later makes
// wsrep::client_state::close() run transaction::after_statement() on a
// non-m_local, still-active transaction and hit
// assert(client_state_.mode() == m_local) -- the cluster lockup described
// in MDEV-38843.
//
// apply_write_set()'s combined start+commit branch was fixed for its
// commit-failure case. This file documents (and, for the still-open
// sites, currently demonstrates the failure of) every other call site
// sharing the same anti-pattern:
//
//   - apply_write_set(): commit() fails            (FIXED)
//   - apply_fragment():  rollback() fails after a failed apply
//   - apply_fragment():  commit() fails while removing fragments
//   - apply_fragment():  append_fragment_and_commit() fails
//   - commit_fragment(): commit() fails on the last fragment
//   - rollback_fragment(): rollback() fails
//

#include "mock_server_state.hpp"

#include <boost/test/unit_test.hpp>

namespace
{
    struct applier_cleanup_fixture
    {
        applier_cleanup_fixture()
            : server_service(&ss)
            , ss("s1", wsrep::server_state::rm_sync, server_service)
            , cc(ss, wsrep::client_id(1), wsrep::client_state::m_high_priority)
            , hps(ss, &cc, false)
            , ws_handle(wsrep::transaction_id(1), (void*)1)
            , ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
                     wsrep::stid(wsrep::id("1"), wsrep::transaction_id(1),
                                 wsrep::client_id(1)),
                     wsrep::seqno(0),
                     wsrep::provider::flag::start_transaction |
                     wsrep::provider::flag::commit)
        {
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            ss.mock_connect();
        }

        wsrep::mock_server_service server_service;
        wsrep::mock_server_state ss;
        wsrep::mock_client cc;
        wsrep::mock_high_priority_service hps;
        wsrep::ws_handle ws_handle;
        wsrep::ws_meta ws_meta;
    };

    // Registers a streaming applier via a single, successfully applied
    // start_transaction fragment and returns it.
    wsrep::mock_high_priority_service* start_streaming_fragment(
        wsrep::mock_server_state& ss,
        wsrep::mock_high_priority_service& hps,
        const wsrep::ws_handle& ws_handle,
        const wsrep::id& server_id,
        wsrep::transaction_id trx_id)
    {
        wsrep::ws_meta start_meta(
            wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
            wsrep::stid(server_id, trx_id, wsrep::client_id(1)),
            wsrep::seqno(0),
            wsrep::provider::flag::start_transaction);
        char buf[1] = { 1 };
        BOOST_REQUIRE(ss.on_apply(hps, ws_handle, start_meta,
                                  wsrep::const_buffer(buf, 1)) == 0);
        wsrep::mock_high_priority_service* sa(
            static_cast<wsrep::mock_high_priority_service*>(
                ss.find_streaming_applier(server_id, trx_id)));
        BOOST_REQUIRE(sa);
        return sa;
    }
}

// apply_write_set(): the combined start+commit branch already rolls back
// and unconditionally runs after_apply() when commit() fails. This is the
// fix that landed for MDEV-38843; kept here as a regression test for that
// fix alongside the still-open sites below.
BOOST_FIXTURE_TEST_CASE(apply_write_set_commit_failure_calls_after_apply,
                        applier_cleanup_fixture)
{
    // Simulate commit order not being enterable, e.g. because the node is
    // leaving the primary component.
    ss.provider().commit_order_enter_result_ = wsrep::provider::error_bf_abort;
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) != 0);
    BOOST_REQUIRE_EQUAL(hps.after_apply_calls_, 1U);
    BOOST_REQUIRE(cc.transaction().active() == false);
}

// apply_fragment(): a fragment fails to apply and the resulting rollback
// of that failed attempt also fails (e.g. a storage-engine level rollback
// failure). after_apply() must still run.
BOOST_FIXTURE_TEST_CASE(apply_fragment_rollback_failure_calls_after_apply,
                        applier_cleanup_fixture)
{
    wsrep::mock_high_priority_service* sa(
        start_streaming_fragment(ss, hps, ws_handle,
                                 wsrep::id("1"), wsrep::transaction_id(1)));

    sa->fail_next_applying_ = true;
    sa->fail_next_rollback_ = true;
    size_t const after_apply_calls_before(sa->after_apply_calls_);

    wsrep::ws_meta mid_meta(
        wsrep::gtid(wsrep::id("1"), wsrep::seqno(2)),
        wsrep::stid(wsrep::id("1"), wsrep::transaction_id(1),
                    wsrep::client_id(1)),
        wsrep::seqno(1),
        0);
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, mid_meta,
                              wsrep::const_buffer(buf, 1)) != 0);
    BOOST_REQUIRE_EQUAL(sa->after_apply_calls_, after_apply_calls_before + 1);
}

// apply_fragment(): a fragment fails to apply, the rollback of that
// attempt succeeds, but the subsequent commit that removes the streaming
// fragments from storage fails. after_apply() must still run.
BOOST_FIXTURE_TEST_CASE(apply_fragment_remove_fragments_commit_failure_calls_after_apply,
                        applier_cleanup_fixture)
{
    wsrep::mock_high_priority_service* sa(
        start_streaming_fragment(ss, hps, ws_handle,
                                 wsrep::id("1"), wsrep::transaction_id(1)));

    sa->fail_next_applying_ = true;
    size_t const after_apply_calls_before(sa->after_apply_calls_);
    // Fail the commit that finalizes fragment removal, e.g. because
    // commit order could not be entered.
    ss.provider().commit_order_enter_result_ = wsrep::provider::error_bf_abort;

    wsrep::ws_meta mid_meta(
        wsrep::gtid(wsrep::id("1"), wsrep::seqno(2)),
        wsrep::stid(wsrep::id("1"), wsrep::transaction_id(1),
                    wsrep::client_id(1)),
        wsrep::seqno(1),
        0);
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, mid_meta,
                              wsrep::const_buffer(buf, 1)) != 0);
    // One after_apply() call is expected from the rollback of the failed
    // apply, and a second one after the (failing) fragment-removal commit.
    BOOST_REQUIRE_EQUAL(sa->after_apply_calls_, after_apply_calls_before + 2);
}

// apply_fragment(): a fragment applies successfully, but
// append_fragment_and_commit() (appending it to persistent fragment
// storage) fails. after_apply() must still run on the coordinating
// high_priority_service so its transaction does not remain active.
BOOST_FIXTURE_TEST_CASE(apply_fragment_append_fragment_failure_calls_after_apply,
                        applier_cleanup_fixture)
{
    hps.fail_next_append_fragment_ = true;
    size_t const after_apply_calls_before(hps.after_apply_calls_);

    wsrep::ws_meta start_meta(
        wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
        wsrep::stid(wsrep::id("1"), wsrep::transaction_id(1),
                    wsrep::client_id(1)),
        wsrep::seqno(0),
        wsrep::provider::flag::start_transaction);
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, start_meta,
                              wsrep::const_buffer(buf, 1)) != 0);
    BOOST_REQUIRE_EQUAL(hps.after_apply_calls_, after_apply_calls_before + 1);
}

// commit_fragment(): the final commit of a streaming (or XA) transaction
// fails, e.g. because commit order could not be entered while the node is
// leaving the primary component -- the streaming-transaction analogue of
// the single-shot case already fixed in apply_write_set(). after_apply()
// must still run.
BOOST_FIXTURE_TEST_CASE(commit_fragment_commit_failure_calls_after_apply,
                        applier_cleanup_fixture)
{
    wsrep::mock_high_priority_service* sa(
        start_streaming_fragment(ss, hps, ws_handle,
                                 wsrep::id("1"), wsrep::transaction_id(1)));

    size_t const after_apply_calls_before(sa->after_apply_calls_);
    ss.provider().commit_order_enter_result_ = wsrep::provider::error_bf_abort;

    wsrep::ws_meta commit_meta(
        wsrep::gtid(wsrep::id("1"), wsrep::seqno(2)),
        wsrep::stid(wsrep::id("1"), wsrep::transaction_id(1),
                    wsrep::client_id(1)),
        wsrep::seqno(1),
        wsrep::provider::flag::commit);
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, commit_meta,
                              wsrep::const_buffer(buf, 1)) != 0);
    BOOST_REQUIRE_EQUAL(sa->after_apply_calls_, after_apply_calls_before + 1);
}

// rollback_fragment(): rolling back an already registered streaming
// applier fails (e.g. a storage-engine level rollback failure).
// after_apply() must still run.
BOOST_FIXTURE_TEST_CASE(rollback_fragment_rollback_failure_calls_after_apply,
                        applier_cleanup_fixture)
{
    wsrep::mock_high_priority_service* sa(
        start_streaming_fragment(ss, hps, ws_handle,
                                 wsrep::id("1"), wsrep::transaction_id(1)));

    sa->fail_next_rollback_ = true;
    size_t const after_apply_calls_before(sa->after_apply_calls_);

    // Use a separate high priority service for the rollback write set,
    // mirroring how a real rollback fragment is delivered on its own
    // applier thread (see wsrep_test::terminate_streaming_applier()).
    wsrep::mock_client rollback_cc(ss, wsrep::client_id(2),
                                   wsrep::client_state::m_high_priority);
    rollback_cc.open(wsrep::client_id(2));
    rollback_cc.before_command();
    wsrep::mock_high_priority_service rollback_hps(ss, &rollback_cc, false);

    wsrep::ws_meta rollback_meta(
        wsrep::gtid(wsrep::id("1"), wsrep::seqno(2)),
        wsrep::stid(wsrep::id("1"), wsrep::transaction_id(1),
                    wsrep::client_id(1)),
        wsrep::seqno(1),
        wsrep::provider::flag::rollback);
    BOOST_REQUIRE(ss.on_apply(rollback_hps, ws_handle, rollback_meta,
                              wsrep::const_buffer(0, 0)) != 0);
    // Non-fatal: the cleanup below must still run when this fails, so a
    // failure here does not also crash the test binary (see comment
    // below).
    BOOST_CHECK_EQUAL(sa->after_apply_calls_, after_apply_calls_before + 1);

    // rollback_fragment() adopts sa's transaction into rollback_cc before
    // attempting the rollback (fragments are pending removal), and since
    // the rollback attempt failed, the remainder of rollback_fragment()
    // that would normally finish that adopted transaction never runs
    // either -- another symptom of the same missing-cleanup defect. Finish
    // it here so tearing down rollback_cc does not itself trip over the
    // abandoned, still-active transaction.
    rollback_hps.rollback(ws_handle, rollback_meta);
    rollback_hps.after_apply();
}
