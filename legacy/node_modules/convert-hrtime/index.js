export default function convertHrtime(hrtime) {
	const nanoseconds = hrtime;
	const number = Number(nanoseconds);
	const milliseconds = number / 1000000;
	const seconds = number / 1000000000;

	return {
		seconds,
		milliseconds,
		nanoseconds
	};
}
