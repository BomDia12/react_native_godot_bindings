#include "../runtime/rn_runtime_scheduler.h"
#include "../singletons/hermes_runtime_singleton.h"

#include "tests/test_macros.h"

namespace TestRNScheduler {
struct Fixture {
	HermesRuntimeSingleton *runtime = HermesRuntimeSingleton::get_singleton();
	double clock = 5000;
	std::shared_ptr<RNRuntimeScheduler> scheduler;
	Fixture() {
		runtime->reset();
		scheduler = std::make_shared<RNRuntimeScheduler>([this] { return clock; });
		runtime->install_host_object("__testScheduler", scheduler);
		runtime->evaluate("globalThis.events=[]; globalThis.s=__testScheduler; undefined;");
	}
	~Fixture() { runtime->uninstall_host_object("__testScheduler"); }
	void frame(bool p_visible = true) { runtime->dispatch_scheduler(scheduler, false, p_visible); }
	String events() { return runtime->evaluate("JSON.stringify(events)"); }
};

TEST_CASE("[ReactNativeBindings][Scheduler] due snapshots cancellation arguments coercion and microtasks") {
	Fixture f;
	f.runtime->evaluate("s.setTimeout((v)=>{events.push(v);s.clearTimeout(cancel);s.setTimeout(()=>events.push('later'),0);Promise.resolve().then(()=>events.push('promise'));},'5','first');var cancel=s.setTimeout(()=>events.push('cancelled'),5);s.setTimeout(()=>events.push('last'),5);undefined;");
	f.frame();
	CHECK(f.events() == "[]");
	f.clock += 5;
	f.frame();
	CHECK(f.events() == "[\"first\",\"promise\",\"last\"]");
	f.frame();
	CHECK(f.events() == "[\"first\",\"promise\",\"last\",\"later\"]");
	CHECK_FALSE(f.scheduler->has_pending_work());
}

TEST_CASE("[ReactNativeBindings][Scheduler] intervals skip missed periods and cancellation inside delivery") {
	Fixture f;
	f.runtime->evaluate("var interval=s.setInterval(()=>{events.push(s.now());if(events.length===2)s.clearInterval(interval);},10);undefined;");
	f.clock += 1000;
	f.frame();
	CHECK(f.events() == "[1000]");
	f.frame();
	CHECK(f.events() == "[1000]");
	f.clock += 10;
	f.frame();
	CHECK(f.events() == "[1000,1010]");
	CHECK_FALSE(f.scheduler->has_pending_work());
}

TEST_CASE("[ReactNativeBindings][Scheduler] frame eligibility idle deadlines bounds reset and errors") {
	Fixture f;
	f.scheduler->configure(2, 0);
	f.runtime->evaluate("s.requestAnimationFrame(time=>events.push(['frame',time]));s.requestIdleCallback(d=>events.push(['idle',d.didTimeout,d.timeRemaining()]),{timeout:5});s.setTimeout(()=>{throw Error('expected');},0);s.setTimeout(()=>events.push('after-error'),0);undefined;");
	ERR_PRINT_OFF;
	f.frame(false);
	ERR_PRINT_ON;
	CHECK(f.events() == "[\"after-error\"]");
	f.runtime->dispatch_scheduler(f.scheduler, true, false);
	CHECK(f.events() == "[\"after-error\"]");
	f.clock += 5;
	f.frame(false);
	f.runtime->dispatch_scheduler(f.scheduler, true, false);
	CHECK(f.events() == "[\"after-error\",[\"idle\",true,0]]");
	f.frame();
	CHECK(f.events() == "[\"after-error\",[\"idle\",true,0],[\"frame\",5]]");
	f.runtime->evaluate("s.setTimeout(()=>events.push('stale'),0);undefined;");
	CHECK(f.scheduler->has_pending_work());
	f.runtime->reset();
	CHECK_FALSE(f.scheduler->has_pending_work());
	CHECK(f.scheduler->now() == 0);
}

TEST_CASE("[ReactNativeBindings][Scheduler] idle budget decreases and new callbacks wait for next turn") {
	Fixture f;
	f.runtime->evaluate("s.requestIdleCallback(d=>{globalThis.deadline=d;events.push(d.didTimeout);s.requestIdleCallback(()=>events.push('nested'));});undefined;");
	f.runtime->dispatch_scheduler(f.scheduler, true, false);
	CHECK(f.events() == "[false]");
	CHECK(double(f.runtime->evaluate("deadline.timeRemaining()")) == 2);
	f.clock += 1;
	CHECK(double(f.runtime->evaluate("deadline.timeRemaining()")) == 1);
	f.clock += 9;
	CHECK(double(f.runtime->evaluate("deadline.timeRemaining()")) == 0);
	f.runtime->dispatch_scheduler(f.scheduler, true, false);
	CHECK(f.events() == "[false,\"nested\"]");
}
TEST_CASE("[ReactNativeBindings][Scheduler] separate origins survive timer delivery and clear before microtasks") {
	Fixture f;
	f.runtime->evaluate("var left={generation:1,rootTag:11,epoch:2};var right={generation:1,rootTag:21,epoch:3};s.withOrigin(left,()=>s.setTimeout(()=>{events.push(s.getOrigin().rootTag);Promise.resolve().then(()=>events.push(s.getOrigin()));},0));s.withOrigin(right,()=>s.setTimeout(()=>events.push(s.getOrigin().rootTag),0));undefined;");
	f.frame();
	CHECK(f.events() == "[11,null,21]");
	CHECK(f.runtime->evaluate("s.getOrigin()").get_type() == Variant::NIL);
	f.runtime->evaluate("globalThis.originRejected=false;try{s.withOrigin({generation:NaN,rootTag:11,epoch:2},()=>{});}catch(error){originRejected=true;}undefined;");
	CHECK(bool(f.runtime->get_global("originRejected")));
}
TEST_CASE("[ReactNativeBindings][Scheduler] available frame time bounds idle work and timeout still runs") {
	Fixture f;
	f.runtime->evaluate("s.requestIdleCallback(d=>events.push(d.timeRemaining()));s.requestIdleCallback(d=>events.push(d.didTimeout),{timeout:1});undefined;");
	f.runtime->dispatch_scheduler(f.scheduler, true, false, 0);
	CHECK(f.events() == "[]");
	f.clock += 1;
	f.runtime->dispatch_scheduler(f.scheduler, true, false, 0);
	CHECK(f.events() == "[true]");
	f.runtime->dispatch_scheduler(f.scheduler, true, false, 0.5);
	CHECK(f.events() == "[true,0.5]");
}
TEST_CASE("[ReactNativeBindings][Scheduler] native callbacks timers and idle share one frame allowance") {
	Fixture f;
	f.scheduler->configure(2, 2);
	f.runtime->evaluate("s.setTimeout(()=>events.push('timer'),0);s.requestIdleCallback(()=>events.push('idle'));undefined;");
	f.runtime->dispatch_scheduler(f.scheduler, false, false, 0, 1);
	f.runtime->dispatch_scheduler(f.scheduler, true, false);
	CHECK(f.events() == "[\"timer\"]");
	f.frame(false);
	f.runtime->dispatch_scheduler(f.scheduler, true, false);
	CHECK(f.events() == "[\"timer\",\"idle\"]");
}
TEST_CASE("[ReactNativeBindings][Scheduler] recurring callbacks rotate behind waiting timers and frames") {
	Fixture f;
	f.scheduler->configure(2, 2);
	f.runtime->evaluate("s.setInterval(()=>events.push('first'),1);s.setInterval(()=>events.push('second'),1);s.setTimeout(()=>events.push('timeout'),0);s.requestAnimationFrame(()=>events.push('frame'));undefined;");
	f.clock += 10;
	f.frame();
	CHECK(f.events() == "[\"first\",\"second\"]");
	f.clock += 10;
	f.frame();
	CHECK(f.events() == "[\"first\",\"second\",\"timeout\",\"frame\"]");
	f.clock += 10;
	f.frame();
	CHECK(f.events() == "[\"first\",\"second\",\"timeout\",\"frame\",\"first\",\"second\"]");
}
} //namespace TestRNScheduler

void rn_force_link_scheduler_tests() {}
