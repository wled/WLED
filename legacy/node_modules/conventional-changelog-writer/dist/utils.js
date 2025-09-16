const DATETIME_LENGTH = 10;
/**
 * Formats date to yyyy-mm-dd format.
 * @param date - Date string or Date object.
 * @returns Date string in yyyy-mm-dd format.
 */
export function formatDate(date) {
    return new Date(date).toISOString().slice(0, DATETIME_LENGTH);
}
/**
 * Safe JSON.stringify with circular reference support.
 * @param obj
 * @returns Stringified object with circular references.
 */
export function stringify(obj) {
    const stack = [];
    const keys = [];
    let thisPos;
    function cycleReplacer(value) {
        if (stack[0] === value) {
            return '[Circular ~]';
        }
        return `[Circular ~.${keys.slice(0, stack.indexOf(value)).join('.')}]`;
    }
    function serializer(key, value) {
        let resultValue = value;
        if (stack.length > 0) {
            thisPos = stack.indexOf(this);
            if (thisPos !== -1) {
                stack.splice(thisPos + 1);
                keys.splice(thisPos, Infinity, key);
            }
            else {
                stack.push(this);
                keys.push(key);
            }
            if (stack.includes(resultValue)) {
                resultValue = cycleReplacer(resultValue);
            }
        }
        else {
            stack.push(resultValue);
        }
        return resultValue;
    }
    return JSON.stringify(obj, serializer, '  ');
}
/**
 * Creates a compare function for sorting from object keys.
 * @param strings - String or array of strings of object keys to compare.
 * @returns Compare function.
 */
export function createComparator(strings) {
    if (typeof strings === 'string') {
        return (a, b) => (a[strings] || '').localeCompare(b[strings] || '');
    }
    if (Array.isArray(strings)) {
        return (a, b) => {
            let strA = '';
            let strB = '';
            for (const key of strings) {
                strA += a[key] || '';
                strB += b[key] || '';
            }
            return strA.localeCompare(strB);
        };
    }
    return strings;
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoidXRpbHMuanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyIuLi9zcmMvdXRpbHMudHMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBS0EsTUFBTSxlQUFlLEdBQUcsRUFBRSxDQUFBO0FBRTFCOzs7O0dBSUc7QUFDSCxNQUFNLFVBQVUsVUFBVSxDQUN4QixJQUFtQjtJQUVuQixPQUFPLElBQUksSUFBSSxDQUFDLElBQUksQ0FBQyxDQUFDLFdBQVcsRUFBRSxDQUFDLEtBQUssQ0FBQyxDQUFDLEVBQUUsZUFBZSxDQUFDLENBQUE7QUFDL0QsQ0FBQztBQUVEOzs7O0dBSUc7QUFDSCxNQUFNLFVBQVUsU0FBUyxDQUFDLEdBQVk7SUFDcEMsTUFBTSxLQUFLLEdBQWMsRUFBRSxDQUFBO0lBQzNCLE1BQU0sSUFBSSxHQUFhLEVBQUUsQ0FBQTtJQUN6QixJQUFJLE9BQWUsQ0FBQTtJQUVuQixTQUFTLGFBQWEsQ0FBQyxLQUFjO1FBQ25DLElBQUksS0FBSyxDQUFDLENBQUMsQ0FBQyxLQUFLLEtBQUssRUFBRTtZQUN0QixPQUFPLGNBQWMsQ0FBQTtTQUN0QjtRQUVELE9BQU8sZUFBZSxJQUFJLENBQUMsS0FBSyxDQUFDLENBQUMsRUFBRSxLQUFLLENBQUMsT0FBTyxDQUFDLEtBQUssQ0FBQyxDQUFDLENBQUMsSUFBSSxDQUFDLEdBQUcsQ0FBQyxHQUFHLENBQUE7SUFDeEUsQ0FBQztJQUVELFNBQVMsVUFBVSxDQUFnQixHQUFXLEVBQUUsS0FBYztRQUM1RCxJQUFJLFdBQVcsR0FBRyxLQUFLLENBQUE7UUFFdkIsSUFBSSxLQUFLLENBQUMsTUFBTSxHQUFHLENBQUMsRUFBRTtZQUNwQixPQUFPLEdBQUcsS0FBSyxDQUFDLE9BQU8sQ0FBQyxJQUFJLENBQUMsQ0FBQTtZQUU3QixJQUFJLE9BQU8sS0FBSyxDQUFDLENBQUMsRUFBRTtnQkFDbEIsS0FBSyxDQUFDLE1BQU0sQ0FBQyxPQUFPLEdBQUcsQ0FBQyxDQUFDLENBQUE7Z0JBQ3pCLElBQUksQ0FBQyxNQUFNLENBQUMsT0FBTyxFQUFFLFFBQVEsRUFBRSxHQUFHLENBQUMsQ0FBQTthQUNwQztpQkFBTTtnQkFDTCxLQUFLLENBQUMsSUFBSSxDQUFDLElBQUksQ0FBQyxDQUFBO2dCQUNoQixJQUFJLENBQUMsSUFBSSxDQUFDLEdBQUcsQ0FBQyxDQUFBO2FBQ2Y7WUFFRCxJQUFJLEtBQUssQ0FBQyxRQUFRLENBQUMsV0FBVyxDQUFDLEVBQUU7Z0JBQy9CLFdBQVcsR0FBRyxhQUFhLENBQUMsV0FBVyxDQUFDLENBQUE7YUFDekM7U0FDRjthQUFNO1lBQ0wsS0FBSyxDQUFDLElBQUksQ0FBQyxXQUFXLENBQUMsQ0FBQTtTQUN4QjtRQUVELE9BQU8sV0FBVyxDQUFBO0lBQ3BCLENBQUM7SUFFRCxPQUFPLElBQUksQ0FBQyxTQUFTLENBQUMsR0FBRyxFQUFFLFVBQVUsRUFBRSxJQUFJLENBQUMsQ0FBQTtBQUM5QyxDQUFDO0FBRUQ7Ozs7R0FJRztBQUNILE1BQU0sVUFBVSxnQkFBZ0IsQ0FHOUIsT0FBNEM7SUFDNUMsSUFBSSxPQUFPLE9BQU8sS0FBSyxRQUFRLEVBQUU7UUFDL0IsT0FBTyxDQUFDLENBQUksRUFBRSxDQUFJLEVBQUUsRUFBRSxDQUFDLENBQUMsQ0FBQyxDQUFDLE9BQU8sQ0FBQyxJQUFJLEVBQUUsQ0FBQyxDQUFDLGFBQWEsQ0FBQyxDQUFDLENBQUMsT0FBTyxDQUFDLElBQUksRUFBRSxDQUFDLENBQUE7S0FDMUU7SUFFRCxJQUFJLEtBQUssQ0FBQyxPQUFPLENBQUMsT0FBTyxDQUFDLEVBQUU7UUFDMUIsT0FBTyxDQUFDLENBQUksRUFBRSxDQUFJLEVBQUUsRUFBRTtZQUNwQixJQUFJLElBQUksR0FBRyxFQUFFLENBQUE7WUFDYixJQUFJLElBQUksR0FBRyxFQUFFLENBQUE7WUFFYixLQUFLLE1BQU0sR0FBRyxJQUFJLE9BQU8sRUFBRTtnQkFDekIsSUFBSSxJQUFJLENBQUMsQ0FBQyxHQUFHLENBQUMsSUFBSSxFQUFFLENBQUE7Z0JBQ3BCLElBQUksSUFBSSxDQUFDLENBQUMsR0FBRyxDQUFDLElBQUksRUFBRSxDQUFBO2FBQ3JCO1lBRUQsT0FBTyxJQUFJLENBQUMsYUFBYSxDQUFDLElBQUksQ0FBQyxDQUFBO1FBQ2pDLENBQUMsQ0FBQTtLQUNGO0lBRUQsT0FBTyxPQUFPLENBQUE7QUFDaEIsQ0FBQyJ9