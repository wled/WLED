const nomatchRegex = /(?!.*)/;
function join(parts, joiner) {
    return parts
        .map(val => val.trim())
        .filter(Boolean)
        .join(joiner);
}
function getNotesRegex(noteKeywords, notesPattern) {
    if (!noteKeywords) {
        return nomatchRegex;
    }
    const noteKeywordsSelection = join(noteKeywords, '|');
    if (!notesPattern) {
        return new RegExp(`^[\\s|*]*(${noteKeywordsSelection})[:\\s]+(.*)`, 'i');
    }
    return notesPattern(noteKeywordsSelection);
}
function getReferencePartsRegex(issuePrefixes, issuePrefixesCaseSensitive) {
    if (!issuePrefixes) {
        return nomatchRegex;
    }
    const flags = issuePrefixesCaseSensitive ? 'g' : 'gi';
    return new RegExp(`(?:.*?)??\\s*([\\w-\\.\\/]*?)??(${join(issuePrefixes, '|')})([\\w-]*\\d+)`, flags);
}
function getReferencesRegex(referenceActions) {
    if (!referenceActions) {
        // matches everything
        return /()(.+)/gi;
    }
    const joinedKeywords = join(referenceActions, '|');
    return new RegExp(`(${joinedKeywords})(?:\\s+(.*?))(?=(?:${joinedKeywords})|$)`, 'gi');
}
/**
 * Make the regexes used to parse a commit.
 * @param options
 * @returns Regexes.
 */
export function getParserRegexes(options = {}) {
    const notes = getNotesRegex(options.noteKeywords, options.notesPattern);
    const referenceParts = getReferencePartsRegex(options.issuePrefixes, options.issuePrefixesCaseSensitive);
    const references = getReferencesRegex(options.referenceActions);
    return {
        notes,
        referenceParts,
        references,
        mentions: /@([\w-]+)/g,
        url: /\b(?:https?):\/\/(?:www\.)?([-a-zA-Z0-9@:%_+.~#?&//=])+\b/
    };
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoicmVnZXguanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyIuLi9zcmMvcmVnZXgudHMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBS0EsTUFBTSxZQUFZLEdBQUcsUUFBUSxDQUFBO0FBRTdCLFNBQVMsSUFBSSxDQUFDLEtBQWUsRUFBRSxNQUFjO0lBQzNDLE9BQU8sS0FBSztTQUNULEdBQUcsQ0FBQyxHQUFHLENBQUMsRUFBRSxDQUFDLEdBQUcsQ0FBQyxJQUFJLEVBQUUsQ0FBQztTQUN0QixNQUFNLENBQUMsT0FBTyxDQUFDO1NBQ2YsSUFBSSxDQUFDLE1BQU0sQ0FBQyxDQUFBO0FBQ2pCLENBQUM7QUFFRCxTQUFTLGFBQWEsQ0FDcEIsWUFBa0MsRUFDbEMsWUFBb0Q7SUFFcEQsSUFBSSxDQUFDLFlBQVksRUFBRTtRQUNqQixPQUFPLFlBQVksQ0FBQTtLQUNwQjtJQUVELE1BQU0scUJBQXFCLEdBQUcsSUFBSSxDQUFDLFlBQVksRUFBRSxHQUFHLENBQUMsQ0FBQTtJQUVyRCxJQUFJLENBQUMsWUFBWSxFQUFFO1FBQ2pCLE9BQU8sSUFBSSxNQUFNLENBQUMsYUFBYSxxQkFBcUIsY0FBYyxFQUFFLEdBQUcsQ0FBQyxDQUFBO0tBQ3pFO0lBRUQsT0FBTyxZQUFZLENBQUMscUJBQXFCLENBQUMsQ0FBQTtBQUM1QyxDQUFDO0FBRUQsU0FBUyxzQkFBc0IsQ0FDN0IsYUFBbUMsRUFDbkMsMEJBQStDO0lBRS9DLElBQUksQ0FBQyxhQUFhLEVBQUU7UUFDbEIsT0FBTyxZQUFZLENBQUE7S0FDcEI7SUFFRCxNQUFNLEtBQUssR0FBRywwQkFBMEIsQ0FBQyxDQUFDLENBQUMsR0FBRyxDQUFDLENBQUMsQ0FBQyxJQUFJLENBQUE7SUFFckQsT0FBTyxJQUFJLE1BQU0sQ0FBQyxtQ0FBbUMsSUFBSSxDQUFDLGFBQWEsRUFBRSxHQUFHLENBQUMsZ0JBQWdCLEVBQUUsS0FBSyxDQUFDLENBQUE7QUFDdkcsQ0FBQztBQUVELFNBQVMsa0JBQWtCLENBQ3pCLGdCQUFzQztJQUV0QyxJQUFJLENBQUMsZ0JBQWdCLEVBQUU7UUFDckIscUJBQXFCO1FBQ3JCLE9BQU8sVUFBVSxDQUFBO0tBQ2xCO0lBRUQsTUFBTSxjQUFjLEdBQUcsSUFBSSxDQUFDLGdCQUFnQixFQUFFLEdBQUcsQ0FBQyxDQUFBO0lBRWxELE9BQU8sSUFBSSxNQUFNLENBQUMsSUFBSSxjQUFjLHVCQUF1QixjQUFjLE1BQU0sRUFBRSxJQUFJLENBQUMsQ0FBQTtBQUN4RixDQUFDO0FBRUQ7Ozs7R0FJRztBQUNILE1BQU0sVUFBVSxnQkFBZ0IsQ0FDOUIsVUFBc0ksRUFBRTtJQUV4SSxNQUFNLEtBQUssR0FBRyxhQUFhLENBQUMsT0FBTyxDQUFDLFlBQVksRUFBRSxPQUFPLENBQUMsWUFBWSxDQUFDLENBQUE7SUFDdkUsTUFBTSxjQUFjLEdBQUcsc0JBQXNCLENBQUMsT0FBTyxDQUFDLGFBQWEsRUFBRSxPQUFPLENBQUMsMEJBQTBCLENBQUMsQ0FBQTtJQUN4RyxNQUFNLFVBQVUsR0FBRyxrQkFBa0IsQ0FBQyxPQUFPLENBQUMsZ0JBQWdCLENBQUMsQ0FBQTtJQUUvRCxPQUFPO1FBQ0wsS0FBSztRQUNMLGNBQWM7UUFDZCxVQUFVO1FBQ1YsUUFBUSxFQUFFLFlBQVk7UUFDdEIsR0FBRyxFQUFFLDJEQUEyRDtLQUNqRSxDQUFBO0FBQ0gsQ0FBQyJ9