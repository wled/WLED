export default async function pReduce(iterable, reducer, initialValue) {
	return new Promise((resolve, reject) => {
		const iterator = iterable[Symbol.iterator]();
		let index = 0;

		const next = async total => {
			const element = iterator.next();

			if (element.done) {
				resolve(total);
				return;
			}

			try {
				const [resolvedTotal, resolvedValue] = await Promise.all([total, element.value]);
				next(reducer(resolvedTotal, resolvedValue, index++));
			} catch (error) {
				reject(error);
			}
		};

		next(initialValue);
	});
}
