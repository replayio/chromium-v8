#ifndef V8_REPLAY_REPLAYIO_H_
#define V8_REPLAY_REPLAYIO_H_

#include "include/v8-promise.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"

namespace v8 {
namespace internal {
class Isolate;
class JSPromise;
class Object;
class PromiseReactionJobTask;
class String;

// Whether the promise reports below are made, on this thread. Defined in
// api.cc.
bool RecordReplayReportsPromises();

// Promise reports to the dependency graph. Defined in debug.cc.
void RecordReplayOnPromiseHook(Isolate* isolate, PromiseHookType type,
                               Handle<JSPromise> promise, Handle<Object> parent);

void AddPromiseDependencyGraphAdoption(Isolate* isolate, Handle<Object> promise,
                                       Handle<Object> adopted);

// At the settle, before its reaction jobs are enqueued. Returns the node ID of
// the settle that triggers the promise's reactions; 0 if not recorded.
int RecordReplayOnPromiseSettle(Isolate* isolate, Handle<JSPromise> promise);

// The value RecordReplayOnPromiseSettle returned for the settled promise.
int RecordReplayPromiseReactionSettleNodeId(Isolate* isolate,
                                            Handle<JSPromise> settled);

// At the enqueue of a reaction job.
void RecordReplayOnPromiseReactionJob(Isolate* isolate,
                                      int reaction_settle_node_id,
                                      Handle<PromiseReactionJobTask> task);
}  // namespace internal

namespace replayio {

// Whether the current point replays, so that values can be recorded/replayed
// and ids handed out consistently.
bool AreEventsAvailable();

v8::internal::Handle<v8::internal::String> RecordReplayStringHandle(
    const char* why, v8::internal::Isolate* isolate,
    v8::internal::Handle<v8::internal::String> input);

v8::internal::MaybeHandle<v8::internal::String> RecordReplayStringHandle(
    const char* why, v8::internal::Isolate* isolate,
    v8::internal::MaybeHandle<v8::internal::String> input);

}  // namespace replayio
}  // namespace v8

#endif  // V8_REPLAY_REPLAYIO_H_
