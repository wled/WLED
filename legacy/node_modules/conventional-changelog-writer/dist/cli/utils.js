import { resolve, extname } from 'path';
import { pathToFileURL } from 'url';
import { readFile } from 'fs/promises';
const NEWLINE = /\r?\n/;
export async function* parseJsonStream(stream) {
    let chunk;
    let payload;
    let buffer = '';
    let json;
    for await (chunk of stream) {
        buffer += chunk.toString();
        if (NEWLINE.test(buffer)) {
            payload = buffer.split(NEWLINE);
            buffer = payload.pop() || '';
            for (json of payload) {
                try {
                    yield JSON.parse(json);
                }
                catch (err) {
                    throw new Error('Failed to split commits', {
                        cause: err
                    });
                }
            }
        }
    }
    if (buffer) {
        try {
            yield JSON.parse(buffer);
        }
        catch (err) {
            throw new Error('Failed to split commits', {
                cause: err
            });
        }
    }
}
export async function* readCommitsFromFiles(files) {
    for (const file of files) {
        try {
            yield JSON.parse(await readFile(file, 'utf8'));
        }
        catch (err) {
            console.warn(`Failed to read file ${file}:\n  ${err}`);
        }
    }
}
export function readCommitsFromStdin() {
    return parseJsonStream(process.stdin);
}
function relativeResolve(filePath) {
    return pathToFileURL(resolve(process.cwd(), filePath));
}
export async function loadDataFile(filePath) {
    const resolvedFilePath = relativeResolve(filePath);
    const ext = extname(resolvedFilePath.toString());
    if (ext === '.json') {
        return JSON.parse(await readFile(resolvedFilePath, 'utf8'));
    }
    // @ts-expect-error Dynamic import actually works with file URLs
    // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
    return (await import(resolvedFilePath)).default;
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoidXRpbHMuanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyIuLi8uLi9zcmMvY2xpL3V0aWxzLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUNBLE9BQU8sRUFBRSxPQUFPLEVBQUUsT0FBTyxFQUFFLE1BQU0sTUFBTSxDQUFBO0FBQ3ZDLE9BQU8sRUFBRSxhQUFhLEVBQUUsTUFBTSxLQUFLLENBQUE7QUFDbkMsT0FBTyxFQUFFLFFBQVEsRUFBRSxNQUFNLGFBQWEsQ0FBQTtBQUV0QyxNQUFNLE9BQU8sR0FBRyxPQUFPLENBQUE7QUFFdkIsTUFBTSxDQUFDLEtBQUssU0FBUyxDQUFDLENBQUMsZUFBZSxDQUFJLE1BQWdCO0lBQ3hELElBQUksS0FBYSxDQUFBO0lBQ2pCLElBQUksT0FBaUIsQ0FBQTtJQUNyQixJQUFJLE1BQU0sR0FBRyxFQUFFLENBQUE7SUFDZixJQUFJLElBQVksQ0FBQTtJQUVoQixJQUFJLEtBQUssRUFBRSxLQUFLLElBQUksTUFBTSxFQUFFO1FBQzFCLE1BQU0sSUFBSSxLQUFLLENBQUMsUUFBUSxFQUFFLENBQUE7UUFFMUIsSUFBSSxPQUFPLENBQUMsSUFBSSxDQUFDLE1BQU0sQ0FBQyxFQUFFO1lBQ3hCLE9BQU8sR0FBRyxNQUFNLENBQUMsS0FBSyxDQUFDLE9BQU8sQ0FBQyxDQUFBO1lBQy9CLE1BQU0sR0FBRyxPQUFPLENBQUMsR0FBRyxFQUFFLElBQUksRUFBRSxDQUFBO1lBRTVCLEtBQUssSUFBSSxJQUFJLE9BQU8sRUFBRTtnQkFDcEIsSUFBSTtvQkFDRixNQUFNLElBQUksQ0FBQyxLQUFLLENBQUMsSUFBSSxDQUFNLENBQUE7aUJBQzVCO2dCQUFDLE9BQU8sR0FBRyxFQUFFO29CQUNaLE1BQU0sSUFBSSxLQUFLLENBQUMseUJBQXlCLEVBQUU7d0JBQ3pDLEtBQUssRUFBRSxHQUFHO3FCQUNYLENBQUMsQ0FBQTtpQkFDSDthQUNGO1NBQ0Y7S0FDRjtJQUVELElBQUksTUFBTSxFQUFFO1FBQ1YsSUFBSTtZQUNGLE1BQU0sSUFBSSxDQUFDLEtBQUssQ0FBQyxNQUFNLENBQU0sQ0FBQTtTQUM5QjtRQUFDLE9BQU8sR0FBRyxFQUFFO1lBQ1osTUFBTSxJQUFJLEtBQUssQ0FBQyx5QkFBeUIsRUFBRTtnQkFDekMsS0FBSyxFQUFFLEdBQUc7YUFDWCxDQUFDLENBQUE7U0FDSDtLQUNGO0FBQ0gsQ0FBQztBQUVELE1BQU0sQ0FBQyxLQUFLLFNBQVMsQ0FBQyxDQUFDLG9CQUFvQixDQUFJLEtBQWU7SUFDNUQsS0FBSyxNQUFNLElBQUksSUFBSSxLQUFLLEVBQUU7UUFDeEIsSUFBSTtZQUNGLE1BQU0sSUFBSSxDQUFDLEtBQUssQ0FBQyxNQUFNLFFBQVEsQ0FBQyxJQUFJLEVBQUUsTUFBTSxDQUFDLENBQU0sQ0FBQTtTQUNwRDtRQUFDLE9BQU8sR0FBRyxFQUFFO1lBQ1osT0FBTyxDQUFDLElBQUksQ0FBQyx1QkFBdUIsSUFBSSxRQUFRLEdBQWEsRUFBRSxDQUFDLENBQUE7U0FDakU7S0FDRjtBQUNILENBQUM7QUFFRCxNQUFNLFVBQVUsb0JBQW9CO0lBQ2xDLE9BQU8sZUFBZSxDQUFJLE9BQU8sQ0FBQyxLQUFLLENBQUMsQ0FBQTtBQUMxQyxDQUFDO0FBRUQsU0FBUyxlQUFlLENBQUMsUUFBZ0I7SUFDdkMsT0FBTyxhQUFhLENBQUMsT0FBTyxDQUFDLE9BQU8sQ0FBQyxHQUFHLEVBQUUsRUFBRSxRQUFRLENBQUMsQ0FBQyxDQUFBO0FBQ3hELENBQUM7QUFFRCxNQUFNLENBQUMsS0FBSyxVQUFVLFlBQVksQ0FBQyxRQUFnQjtJQUNqRCxNQUFNLGdCQUFnQixHQUFHLGVBQWUsQ0FBQyxRQUFRLENBQUMsQ0FBQTtJQUNsRCxNQUFNLEdBQUcsR0FBRyxPQUFPLENBQUMsZ0JBQWdCLENBQUMsUUFBUSxFQUFFLENBQUMsQ0FBQTtJQUVoRCxJQUFJLEdBQUcsS0FBSyxPQUFPLEVBQUU7UUFDbkIsT0FBTyxJQUFJLENBQUMsS0FBSyxDQUFDLE1BQU0sUUFBUSxDQUFDLGdCQUFnQixFQUFFLE1BQU0sQ0FBQyxDQUFXLENBQUE7S0FDdEU7SUFFRCxnRUFBZ0U7SUFDaEUsc0VBQXNFO0lBQ3RFLE9BQU8sQ0FBQyxNQUFNLE1BQU0sQ0FBQyxnQkFBZ0IsQ0FBQyxDQUFDLENBQUMsT0FBaUIsQ0FBQTtBQUMzRCxDQUFDIn0=