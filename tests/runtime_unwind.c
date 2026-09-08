#include "runtime_unwind.h"
#include <assert.h>
#include <stdio.h>

static void record(void *opaque, HhyUnwindAction action) {
    unsigned *calls = opaque;
    assert(action == (HhyUnwindAction)*calls);
    (*calls)++;
}

static void record_call(void *opaque, HhyCallUnwindAction action) {
    unsigned *calls = opaque;
    assert(action == (HhyCallUnwindAction)*calls);
    (*calls)++;
}

int main(void) {
    unsigned calls = 0;
    HhyUnwindTable table = hhy_runtime_boundary_unwind;
    assert(hhy_runtime_unwind_apply(&table, &calls, record));
    assert(calls == HHY_UNWIND_ACTION_COUNT);
    for (unsigned i = 0; i < HHY_UNWIND_ACTION_COUNT; i++) {
        table = hhy_runtime_boundary_unwind;
        table.actions[i] = HHY_UNWIND_ACTION_COUNT;
        calls = 0;
        assert(!hhy_runtime_unwind_apply(&table, &calls, record) && calls == 0);
    }
    table = hhy_runtime_boundary_unwind;
    table.actions[0] = HHY_UNWIND_EXTENSION_SCOPE;
    calls = 0;
    assert(!hhy_runtime_unwind_apply(&table, &calls, record) && calls == 0);
    table = hhy_runtime_boundary_unwind;
    table.version++;
    assert(!hhy_runtime_unwind_apply(&table, &calls, record) && calls == 0);
    table = hhy_runtime_boundary_unwind;
    table.count--;
    assert(!hhy_runtime_unwind_apply(&table, &calls, record) && calls == 0);
    assert(!hhy_runtime_unwind_apply(NULL, &calls, record));
    assert(!hhy_runtime_unwind_apply(&hhy_runtime_boundary_unwind, &calls, NULL));
    HhyCallUnwindTable frame = hhy_runtime_call_unwind;
    calls = 0;
    assert(hhy_call_unwind_apply(&frame, &calls, record_call));
    assert(calls == HHY_CALL_UNWIND_ACTION_COUNT);
    for (unsigned i = 0; i < HHY_CALL_UNWIND_ACTION_COUNT; i++) {
        frame = hhy_runtime_call_unwind; frame.actions[i] = HHY_CALL_UNWIND_ACTION_COUNT;
        calls = 0; assert(!hhy_call_unwind_apply(&frame, &calls, record_call) && calls == 0);
    }
    frame = hhy_runtime_call_unwind; frame.version++;
    assert(!hhy_call_unwind_apply(&frame, &calls, record_call) && calls == 0);
    frame = hhy_runtime_call_unwind; frame.count--;
    assert(!hhy_call_unwind_apply(&frame, &calls, record_call) && calls == 0);
    frame = hhy_runtime_call_unwind; frame.actions[0] = HHY_CALL_UNWIND_ROOTS;
    assert(!hhy_call_unwind_apply(&frame, &calls, record_call) && calls == 0);
    assert(!hhy_call_unwind_apply(NULL, &calls, record_call));
    assert(!hhy_call_unwind_apply(&hhy_runtime_call_unwind, &calls, NULL));
    puts("Runtime boundary/call unwind verifier and ordered cleanup passed");
    return 0;
}
