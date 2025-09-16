// Even though the browser version is a no-op, we wrap it to ensure consistent behavior.
export default function functionTimeout(function_) {
	const wrappedFunction = (...arguments_) => function_(...arguments_);

	Object.defineProperty(wrappedFunction, 'name', {
		value: `functionTimeout(${function_.name || '<anonymous>'})`,
		configurable: true,
	});

	return wrappedFunction;
}

export function isTimeoutError() {
	return false;
}
