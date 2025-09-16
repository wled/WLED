import vm from 'node:vm';

const script = new vm.Script('returnValue = functionToRun()');

// TODO: Document the `context` option and add to types when I know it's something I want to keep.

// If you use the `context` option, you do it at your own risk.
export default function functionTimeout(function_, {timeout, context = vm.createContext()} = {}) {
	const wrappedFunction = (...arguments_) => {
		context.functionToRun = () => function_(...arguments_);
		script.runInNewContext(context, {timeout});
		return context.returnValue;
	};

	Object.defineProperty(wrappedFunction, 'name', {
		value: `functionTimeout(${function_.name || '<anonymous>'})`,
		configurable: true,
	});

	return wrappedFunction;
}

export function isTimeoutError(error) {
	return error?.code === 'ERR_SCRIPT_EXECUTION_TIMEOUT';
}
