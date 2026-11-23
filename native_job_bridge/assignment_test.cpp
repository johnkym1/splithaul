#include <cassert>
#include <cstdio>
#include "assignment_state.h"

static scs_named_value_t stringAttribute(const char* name,const char* value) {
    scs_named_value_t a={};a.name=name;a.value.type=SCS_VALUE_TYPE_string;a.value.value_string.value=value;return a;
}
int main() {
    scs_named_value_t a[8]={};
    a[0]=stringAttribute("cargo.id","empty_palet");
    a[1]=stringAttribute("source.city.id","yakima");
    a[2]=stringAttribute("source.company.id","bn_farm");
    a[3]=stringAttribute("destination.city.id","wenatchee");
    a[4]=stringAttribute("destination.company.id","sg_whs");
    a[5]=stringAttribute("job.market","cargo_market");
    a[6].name="cargo.loaded";a[6].value.type=SCS_VALUE_TYPE_bool;a[6].value.value_bool.value=0;
    AssignmentTelemetry state;
    assert(!state.confirmedUnloaded(0));state.update(a);
    assert(state.confirmedUnloaded(0));assert(!state.confirmedUnloaded(state.revision));
    a[6].value.value_bool.value=1;state.update(a);assert(!state.confirmedUnloaded(0));
    a[6].value.value_bool.value=0;a[5].value.value_string.value="freight_market";
    state.update(a);assert(!state.confirmedUnloaded(0));
    a[5].value.value_string.value="cargo_market";a[3].value.value_string.value="olympia";
    state.update(a);assert(!state.confirmedUnloaded(0));
    a[3].value.value_string.value="wenatchee";a[6].value.type=SCS_VALUE_TYPE_INVALID;
    state.update(a);assert(!state.confirmedUnloaded(0));
    state.update(nullptr);assert(!state.active && !state.matches && !state.confirmedUnloaded(0));
    puts("PASS: fresh matching unloaded Cargo Market telemetry required; stale, loaded, wrong market/route and missing values rejected.");
}
