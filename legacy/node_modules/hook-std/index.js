import process from 'node:process';
import {Buffer} from 'node:buffer';

const hook = (stream, options, transform) => {
	if (typeof options !== 'object') {
		transform = options;
		options = {};
	}

	options = {
		silent: true,
		once: false,
		...options,
	};

	let unhookFunction;

	const promise = new Promise(resolve => {
		const {write} = stream;

		const unhook = () => {
			stream.write = write;
			resolve();
		};

		stream.write = (output, encoding, callback) => {
			const callbackReturnValue = transform(String(output), unhook);

			if (options.once) {
				unhook();
			}

			if (options.silent) {
				return typeof callbackReturnValue === 'boolean' ? callbackReturnValue : true;
			}

			let returnValue;
			if (typeof callbackReturnValue === 'string') {
				returnValue = typeof encoding === 'string' ? Buffer.from(callbackReturnValue).toString(encoding) : callbackReturnValue;
			}

			returnValue = returnValue || (Buffer.isBuffer(callbackReturnValue) ? callbackReturnValue : output);

			return write.call(stream, returnValue, encoding, callback);
		};

		unhookFunction = unhook;
	});

	promise.unhook = unhookFunction;

	return promise;
};

export function hookStd(options, transform) {
	const streams = options.streams || [process.stdout, process.stderr];
	const streamPromises = streams.map(stream => hook(stream, options, transform));

	const promise = Promise.all(streamPromises);
	promise.unhook = () => {
		for (const streamPromise of streamPromises) {
			streamPromise.unhook();
		}
	};

	return promise;
}

export function hookStdout(...arguments_) {
	return hook(process.stdout, ...arguments_);
}

export function hookStderr(...arguments_) {
	return hook(process.stderr, ...arguments_);
}
