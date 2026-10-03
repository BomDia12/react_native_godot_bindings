#include "rn_execution_scope.h"

thread_local unsigned RNJSNativeCallScope::depth = 0;
thread_local RNExecutionOrigin RNExecutionScope::origin;
RNExecutionScope::RNExecutionScope(const RNExecutionOrigin &p_origin) : previous(origin) {
	origin = p_origin;
}
RNExecutionScope::~RNExecutionScope() {
	origin = previous;
}
