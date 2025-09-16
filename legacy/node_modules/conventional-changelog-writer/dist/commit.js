function preventModifications(object) {
    return new Proxy(object, {
        get(target, prop) {
            const value = target[prop];
            // https://github.com/conventional-changelog/conventional-changelog/pull/1285
            if (value instanceof Date) {
                return value;
            }
            if (typeof value === 'object' && value !== null) {
                return preventModifications(value);
            }
            return value;
        },
        set() {
            throw new Error('Cannot modify immutable object.');
        },
        deleteProperty() {
            throw new Error('Cannot modify immutable object.');
        }
    });
}
/**
 * Apply transformation to commit.
 * @param commit
 * @param transform
 * @param context
 * @param options
 * @returns Transformed commit.
 */
export async function transformCommit(commit, transform, context, options) {
    let patch = {};
    if (typeof transform === 'function') {
        patch = await transform(preventModifications(commit), context, options);
        if (!patch) {
            return null;
        }
    }
    return {
        ...commit,
        ...patch,
        raw: commit
    };
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiY29tbWl0LmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsiLi4vc3JjL2NvbW1pdC50cyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiQUFTQSxTQUFTLG9CQUFvQixDQUFzQixNQUFTO0lBQzFELE9BQU8sSUFBSSxLQUFLLENBQUMsTUFBTSxFQUFFO1FBQ3ZCLEdBQUcsQ0FBQyxNQUFNLEVBQUUsSUFBWTtZQUN0QixNQUFNLEtBQUssR0FBRyxNQUFNLENBQUMsSUFBSSxDQUFZLENBQUE7WUFFckMsNkVBQTZFO1lBQzdFLElBQUksS0FBSyxZQUFZLElBQUksRUFBRTtnQkFDekIsT0FBTyxLQUFLLENBQUE7YUFDYjtZQUVELElBQUksT0FBTyxLQUFLLEtBQUssUUFBUSxJQUFJLEtBQUssS0FBSyxJQUFJLEVBQUU7Z0JBQy9DLE9BQU8sb0JBQW9CLENBQUMsS0FBSyxDQUFDLENBQUE7YUFDbkM7WUFFRCxPQUFPLEtBQUssQ0FBQTtRQUNkLENBQUM7UUFDRCxHQUFHO1lBQ0QsTUFBTSxJQUFJLEtBQUssQ0FBQyxpQ0FBaUMsQ0FBQyxDQUFBO1FBQ3BELENBQUM7UUFDRCxjQUFjO1lBQ1osTUFBTSxJQUFJLEtBQUssQ0FBQyxpQ0FBaUMsQ0FBQyxDQUFBO1FBQ3BELENBQUM7S0FDRixDQUFDLENBQUE7QUFDSixDQUFDO0FBRUQ7Ozs7Ozs7R0FPRztBQUNILE1BQU0sQ0FBQyxLQUFLLFVBQVUsZUFBZSxDQUNuQyxNQUFjLEVBQ2QsU0FBNkQsRUFDN0QsT0FBNkIsRUFDN0IsT0FBNkI7SUFFN0IsSUFBSSxLQUFLLEdBQTJCLEVBQUUsQ0FBQTtJQUV0QyxJQUFJLE9BQU8sU0FBUyxLQUFLLFVBQVUsRUFBRTtRQUNuQyxLQUFLLEdBQUcsTUFBTSxTQUFTLENBQUMsb0JBQW9CLENBQUMsTUFBTSxDQUFDLEVBQUUsT0FBTyxFQUFFLE9BQU8sQ0FBQyxDQUFBO1FBRXZFLElBQUksQ0FBQyxLQUFLLEVBQUU7WUFDVixPQUFPLElBQUksQ0FBQTtTQUNaO0tBQ0Y7SUFFRCxPQUFPO1FBQ0wsR0FBRyxNQUFNO1FBQ1QsR0FBRyxLQUFLO1FBQ1IsR0FBRyxFQUFFLE1BQU07S0FDWixDQUFBO0FBQ0gsQ0FBQyJ9