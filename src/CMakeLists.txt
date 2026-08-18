#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

add_library(wsrep-lib
  allowlist_service_v1.cpp
  assert.cpp
  client_state.cpp
  config_service_v1.cpp
  connection_monitor_service_v1.cpp
  event_service_v1.cpp
  exception.cpp
  gtid.cpp
  id.cpp
  key.cpp
  logger.cpp
  provider.cpp
  provider_options.cpp
  reporter.cpp
  seqno.cpp
  server_state.cpp
  sr_key_set.cpp
  streaming_context.cpp
  thread.cpp
  thread_service_v1.cpp
  tls_service_v1.cpp
  transaction.cpp
  uuid.cpp
  view.cpp
  wsrep_provider_v26.cpp
  xid.cpp
  )
target_link_libraries(wsrep-lib wsrep_api_v26 pthread ${WSREP_LIB_LIBDL})
