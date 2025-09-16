export const defaultOptions = {
    noteKeywords: ['BREAKING CHANGE', 'BREAKING-CHANGE'],
    issuePrefixes: ['#'],
    referenceActions: [
        'close',
        'closes',
        'closed',
        'fix',
        'fixes',
        'fixed',
        'resolve',
        'resolves',
        'resolved'
    ],
    headerPattern: /^(\w*)(?:\(([\w$@.\-*/ ]*)\))?: (.*)$/,
    headerCorrespondence: [
        'type',
        'scope',
        'subject'
    ],
    revertPattern: /^Revert\s"([\s\S]*)"\s*This reverts commit (\w*)\./,
    revertCorrespondence: ['header', 'hash'],
    fieldPattern: /^-(.*?)-$/
};
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoib3B0aW9ucy5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbIi4uL3NyYy9vcHRpb25zLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUVBLE1BQU0sQ0FBQyxNQUFNLGNBQWMsR0FBa0I7SUFDM0MsWUFBWSxFQUFFLENBQUMsaUJBQWlCLEVBQUUsaUJBQWlCLENBQUM7SUFDcEQsYUFBYSxFQUFFLENBQUMsR0FBRyxDQUFDO0lBQ3BCLGdCQUFnQixFQUFFO1FBQ2hCLE9BQU87UUFDUCxRQUFRO1FBQ1IsUUFBUTtRQUNSLEtBQUs7UUFDTCxPQUFPO1FBQ1AsT0FBTztRQUNQLFNBQVM7UUFDVCxVQUFVO1FBQ1YsVUFBVTtLQUNYO0lBQ0QsYUFBYSxFQUFFLHVDQUF1QztJQUN0RCxvQkFBb0IsRUFBRTtRQUNwQixNQUFNO1FBQ04sT0FBTztRQUNQLFNBQVM7S0FDVjtJQUNELGFBQWEsRUFBRSxvREFBb0Q7SUFDbkUsb0JBQW9CLEVBQUUsQ0FBQyxRQUFRLEVBQUUsTUFBTSxDQUFDO0lBQ3hDLFlBQVksRUFBRSxXQUFXO0NBQzFCLENBQUEifQ==