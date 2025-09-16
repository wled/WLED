export default function timeSpan() {
	const start = performance.now();

	const end = () => performance.now() - start;
	end.rounded = () => Math.round(end());
	end.seconds = () => end() / 1000;
	end.nanoseconds = () => end() * 1000000;

	return end;
}
