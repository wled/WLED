const SCISSOR = '# ------------------------ >8 ------------------------';
/**
 * Remove leading and trailing newlines.
 * @param input
 * @returns String without leading and trailing newlines.
 */
export function trimNewLines(input) {
    // To escape ReDos we should escape String#replace with regex.
    const matches = input.match(/[^\r\n]/);
    if (typeof matches?.index !== 'number') {
        return '';
    }
    const firstIndex = matches.index;
    let lastIndex = input.length - 1;
    while (input[lastIndex] === '\r' || input[lastIndex] === '\n') {
        lastIndex--;
    }
    return input.substring(firstIndex, lastIndex + 1);
}
/**
 * Append a newline to a string.
 * @param src
 * @param line
 * @returns String with appended newline.
 */
export function appendLine(src, line) {
    return src ? `${src}\n${line || ''}` : line || '';
}
/**
 * Creates a function that filters out comments lines.
 * @param char
 * @returns Comment filter function.
 */
export function getCommentFilter(char) {
    return char
        ? (line) => !line.startsWith(char)
        : () => true;
}
/**
 * Select lines before the scissor.
 * @param lines
 * @returns Lines before the scissor.
 */
export function truncateToScissor(lines) {
    const scissorIndex = lines.indexOf(SCISSOR);
    if (scissorIndex === -1) {
        return lines;
    }
    return lines.slice(0, scissorIndex);
}
/**
 * Filter out GPG sign lines.
 * @param line
 * @returns True if the line is not a GPG sign line.
 */
export function gpgFilter(line) {
    return !line.match(/^\s*gpg:/);
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoidXRpbHMuanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyIuLi9zcmMvdXRpbHMudHMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUEsTUFBTSxPQUFPLEdBQUcsd0RBQXdELENBQUE7QUFFeEU7Ozs7R0FJRztBQUNILE1BQU0sVUFBVSxZQUFZLENBQUMsS0FBYTtJQUN4Qyw4REFBOEQ7SUFFOUQsTUFBTSxPQUFPLEdBQUcsS0FBSyxDQUFDLEtBQUssQ0FBQyxTQUFTLENBQUMsQ0FBQTtJQUV0QyxJQUFJLE9BQU8sT0FBTyxFQUFFLEtBQUssS0FBSyxRQUFRLEVBQUU7UUFDdEMsT0FBTyxFQUFFLENBQUE7S0FDVjtJQUVELE1BQU0sVUFBVSxHQUFHLE9BQU8sQ0FBQyxLQUFLLENBQUE7SUFDaEMsSUFBSSxTQUFTLEdBQUcsS0FBSyxDQUFDLE1BQU0sR0FBRyxDQUFDLENBQUE7SUFFaEMsT0FBTyxLQUFLLENBQUMsU0FBUyxDQUFDLEtBQUssSUFBSSxJQUFJLEtBQUssQ0FBQyxTQUFTLENBQUMsS0FBSyxJQUFJLEVBQUU7UUFDN0QsU0FBUyxFQUFFLENBQUE7S0FDWjtJQUVELE9BQU8sS0FBSyxDQUFDLFNBQVMsQ0FBQyxVQUFVLEVBQUUsU0FBUyxHQUFHLENBQUMsQ0FBQyxDQUFBO0FBQ25ELENBQUM7QUFFRDs7Ozs7R0FLRztBQUNILE1BQU0sVUFBVSxVQUFVLENBQUMsR0FBa0IsRUFBRSxJQUF3QjtJQUNyRSxPQUFPLEdBQUcsQ0FBQyxDQUFDLENBQUMsR0FBRyxHQUFHLEtBQUssSUFBSSxJQUFJLEVBQUUsRUFBRSxDQUFDLENBQUMsQ0FBQyxJQUFJLElBQUksRUFBRSxDQUFBO0FBQ25ELENBQUM7QUFFRDs7OztHQUlHO0FBQ0gsTUFBTSxVQUFVLGdCQUFnQixDQUFDLElBQXdCO0lBQ3ZELE9BQU8sSUFBSTtRQUNULENBQUMsQ0FBQyxDQUFDLElBQVksRUFBRSxFQUFFLENBQUMsQ0FBQyxJQUFJLENBQUMsVUFBVSxDQUFDLElBQUksQ0FBQztRQUMxQyxDQUFDLENBQUMsR0FBRyxFQUFFLENBQUMsSUFBSSxDQUFBO0FBQ2hCLENBQUM7QUFFRDs7OztHQUlHO0FBQ0gsTUFBTSxVQUFVLGlCQUFpQixDQUFDLEtBQWU7SUFDL0MsTUFBTSxZQUFZLEdBQUcsS0FBSyxDQUFDLE9BQU8sQ0FBQyxPQUFPLENBQUMsQ0FBQTtJQUUzQyxJQUFJLFlBQVksS0FBSyxDQUFDLENBQUMsRUFBRTtRQUN2QixPQUFPLEtBQUssQ0FBQTtLQUNiO0lBRUQsT0FBTyxLQUFLLENBQUMsS0FBSyxDQUFDLENBQUMsRUFBRSxZQUFZLENBQUMsQ0FBQTtBQUNyQyxDQUFDO0FBRUQ7Ozs7R0FJRztBQUNILE1BQU0sVUFBVSxTQUFTLENBQUMsSUFBWTtJQUNwQyxPQUFPLENBQUMsSUFBSSxDQUFDLEtBQUssQ0FBQyxVQUFVLENBQUMsQ0FBQTtBQUNoQyxDQUFDIn0=