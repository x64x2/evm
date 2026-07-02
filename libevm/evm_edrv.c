#include "evm_internal.h"

evm_result_t EVM_EXPORT evm_GetExecutionDriver(const char *driverName, evm_executiondriver_t *driver)
{
#ifndef EVM_NO_JIT
	if(!strcmp(driverName, EVM_DRIVERNAME_NATIVE))
	{
		driver->startCall = evm_vmGenericCall;
		driver->endCall = evm_vmGenericEndCall;
		driver->initializeThread = evm_vmGenericInitializeThread;
		driver->run = evm_vmJITRun;
		driver->compile = evm_vmJITCompileNative;
		return EVM_OK;
	}
#endif

	if(!strcmp(driverName, EVM_DRIVERNAME_INTERPRETER))
	{
		driver->startCall = evm_vmGenericCall;
		driver->endCall = evm_vmGenericEndCall;
		driver->initializeThread = evm_vmGenericInitializeThread;
		driver->run = evm_vmInterpretRun;
		driver->compile = evm_vmInterpretCompileNative;
		return EVM_OK;
	}

	return EVM_ERR_NO_SUCH_EXECUTION_DRIVER;
}
