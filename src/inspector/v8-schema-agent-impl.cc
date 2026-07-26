// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-schema-agent-impl.h"

#include "src/base/replayio.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/v8-inspector-session-impl.h"

namespace v8_inspector {

V8SchemaAgentImpl::V8SchemaAgentImpl(V8InspectorSessionImpl* session,
                                     protocol::FrontendChannel* frontendChannel,
                                     protocol::DictionaryValue* state)
    : m_session(session),
      m_frontend(frontendChannel),
      m_replay_owned(session->replayOwned()) {}

V8SchemaAgentImpl::~V8SchemaAgentImpl() = default;

Response V8SchemaAgentImpl::getDomains(
    std::unique_ptr<protocol::Array<protocol::Schema::Domain>>* result) {
  v8::replayio::AutoMaybeReplayOwned replay_owned(
      m_replay_owned, m_session->inspector()->isolate(),
      "V8SchemaAgentImpl::getDomains");
  *result =
      std::make_unique<std::vector<std::unique_ptr<protocol::Schema::Domain>>>(
          m_session->supportedDomainsImpl());
  return Response::Success();
}

}  // namespace v8_inspector
