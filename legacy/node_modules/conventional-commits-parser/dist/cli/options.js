function trim(str) {
    return str.trim();
}
export function parseOptions(options) {
    if (typeof options.headerPattern === 'string') {
        options.headerPattern = new RegExp(options.headerPattern);
    }
    if (typeof options.headerCorrespondence === 'string') {
        options.headerCorrespondence = options.headerCorrespondence.split(',').map(trim);
    }
    if (typeof options.referenceActions === 'string') {
        options.referenceActions = options.referenceActions.split(',').map(trim);
    }
    if (typeof options.issuePrefixes === 'string') {
        options.issuePrefixes = options.issuePrefixes.split(',').map(trim);
    }
    if (typeof options.noteKeywords === 'string') {
        options.noteKeywords = options.noteKeywords.split(',').map(trim);
    }
    if (typeof options.fieldPattern === 'string') {
        options.fieldPattern = new RegExp(options.fieldPattern);
    }
    if (typeof options.revertPattern === 'string') {
        options.revertPattern = new RegExp(options.revertPattern);
    }
    if (typeof options.revertCorrespondence === 'string') {
        options.revertCorrespondence = options.revertCorrespondence.split(',').map(trim);
    }
    if (typeof options.mergePattern === 'string') {
        options.mergePattern = new RegExp(options.mergePattern);
    }
    if (options.verbose) {
        options.warn = console.warn.bind(console);
    }
    else {
        options.warn = true;
    }
    return options;
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoib3B0aW9ucy5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbIi4uLy4uL3NyYy9jbGkvb3B0aW9ucy50cyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiQUFFQSxTQUFTLElBQUksQ0FBQyxHQUFXO0lBQ3ZCLE9BQU8sR0FBRyxDQUFDLElBQUksRUFBRSxDQUFBO0FBQ25CLENBQUM7QUFFRCxNQUFNLFVBQVUsWUFBWSxDQUFDLE9BQWdDO0lBQzNELElBQUksT0FBTyxPQUFPLENBQUMsYUFBYSxLQUFLLFFBQVEsRUFBRTtRQUM3QyxPQUFPLENBQUMsYUFBYSxHQUFHLElBQUksTUFBTSxDQUFDLE9BQU8sQ0FBQyxhQUFhLENBQUMsQ0FBQTtLQUMxRDtJQUVELElBQUksT0FBTyxPQUFPLENBQUMsb0JBQW9CLEtBQUssUUFBUSxFQUFFO1FBQ3BELE9BQU8sQ0FBQyxvQkFBb0IsR0FBRyxPQUFPLENBQUMsb0JBQW9CLENBQUMsS0FBSyxDQUFDLEdBQUcsQ0FBQyxDQUFDLEdBQUcsQ0FBQyxJQUFJLENBQUMsQ0FBQTtLQUNqRjtJQUVELElBQUksT0FBTyxPQUFPLENBQUMsZ0JBQWdCLEtBQUssUUFBUSxFQUFFO1FBQ2hELE9BQU8sQ0FBQyxnQkFBZ0IsR0FBRyxPQUFPLENBQUMsZ0JBQWdCLENBQUMsS0FBSyxDQUFDLEdBQUcsQ0FBQyxDQUFDLEdBQUcsQ0FBQyxJQUFJLENBQUMsQ0FBQTtLQUN6RTtJQUVELElBQUksT0FBTyxPQUFPLENBQUMsYUFBYSxLQUFLLFFBQVEsRUFBRTtRQUM3QyxPQUFPLENBQUMsYUFBYSxHQUFHLE9BQU8sQ0FBQyxhQUFhLENBQUMsS0FBSyxDQUFDLEdBQUcsQ0FBQyxDQUFDLEdBQUcsQ0FBQyxJQUFJLENBQUMsQ0FBQTtLQUNuRTtJQUVELElBQUksT0FBTyxPQUFPLENBQUMsWUFBWSxLQUFLLFFBQVEsRUFBRTtRQUM1QyxPQUFPLENBQUMsWUFBWSxHQUFHLE9BQU8sQ0FBQyxZQUFZLENBQUMsS0FBSyxDQUFDLEdBQUcsQ0FBQyxDQUFDLEdBQUcsQ0FBQyxJQUFJLENBQUMsQ0FBQTtLQUNqRTtJQUVELElBQUksT0FBTyxPQUFPLENBQUMsWUFBWSxLQUFLLFFBQVEsRUFBRTtRQUM1QyxPQUFPLENBQUMsWUFBWSxHQUFHLElBQUksTUFBTSxDQUFDLE9BQU8sQ0FBQyxZQUFZLENBQUMsQ0FBQTtLQUN4RDtJQUVELElBQUksT0FBTyxPQUFPLENBQUMsYUFBYSxLQUFLLFFBQVEsRUFBRTtRQUM3QyxPQUFPLENBQUMsYUFBYSxHQUFHLElBQUksTUFBTSxDQUFDLE9BQU8sQ0FBQyxhQUFhLENBQUMsQ0FBQTtLQUMxRDtJQUVELElBQUksT0FBTyxPQUFPLENBQUMsb0JBQW9CLEtBQUssUUFBUSxFQUFFO1FBQ3BELE9BQU8sQ0FBQyxvQkFBb0IsR0FBRyxPQUFPLENBQUMsb0JBQW9CLENBQUMsS0FBSyxDQUFDLEdBQUcsQ0FBQyxDQUFDLEdBQUcsQ0FBQyxJQUFJLENBQUMsQ0FBQTtLQUNqRjtJQUVELElBQUksT0FBTyxPQUFPLENBQUMsWUFBWSxLQUFLLFFBQVEsRUFBRTtRQUM1QyxPQUFPLENBQUMsWUFBWSxHQUFHLElBQUksTUFBTSxDQUFDLE9BQU8sQ0FBQyxZQUFZLENBQUMsQ0FBQTtLQUN4RDtJQUVELElBQUksT0FBTyxDQUFDLE9BQU8sRUFBRTtRQUNuQixPQUFPLENBQUMsSUFBSSxHQUFHLE9BQU8sQ0FBQyxJQUFJLENBQUMsSUFBSSxDQUFDLE9BQU8sQ0FBQyxDQUFBO0tBQzFDO1NBQU07UUFDTCxPQUFPLENBQUMsSUFBSSxHQUFHLElBQUksQ0FBQTtLQUNwQjtJQUVELE9BQU8sT0FBTyxDQUFBO0FBQ2hCLENBQUMifQ==