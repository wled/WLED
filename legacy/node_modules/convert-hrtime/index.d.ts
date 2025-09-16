export interface HighResolutionTime {
	seconds: number;
	milliseconds: number;
	nanoseconds: bigint;
}

/**
Convert the result of [`process.hrtime.bigint()`](https://nodejs.org/api/process.html#process_process_hrtime_bigint) to seconds, milliseconds, nanoseconds.

@example
```
import convertHrtime from 'convert-hrtime';

const startTime = process.hrtime.bigint();
expensiveCalculation();
const diff = process.hrtime.bigint() - startTime;

convertHrtime(diff);
//=> {seconds: 0.000002399, milliseconds: 0.002399, nanoseconds: 2399n}
```
*/
export default function convertHrtime(hrtime: bigint): HighResolutionTime;
