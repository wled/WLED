import { findRevertCommit } from './utils.js';
export class RevertedCommitsFilter {
    hold = new Set();
    holdRevertsCount = 0;
    /**
     * Process commit to filter reverted commits
     * @param commit
     * @yields Commit
     */
    *process(commit) {
        const { hold } = this;
        const revertCommit = findRevertCommit(commit, hold);
        if (revertCommit) {
            hold.delete(revertCommit);
            this.holdRevertsCount--;
            return;
        }
        if (commit.revert) {
            hold.add(commit);
            this.holdRevertsCount++;
            return;
        }
        if (this.holdRevertsCount > 0) {
            hold.add(commit);
        }
        else {
            if (hold.size) {
                yield* hold;
                hold.clear();
            }
            yield commit;
        }
    }
    /**
     * Flush all held commits
     * @yields Held commits
     */
    *flush() {
        const { hold } = this;
        if (hold.size) {
            yield* hold;
            hold.clear();
        }
    }
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiUmV2ZXJ0ZWRDb21taXRzRmlsdGVyLmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsiLi4vc3JjL1JldmVydGVkQ29tbWl0c0ZpbHRlci50cyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiQUFDQSxPQUFPLEVBQUUsZ0JBQWdCLEVBQUUsTUFBTSxZQUFZLENBQUE7QUFFN0MsTUFBTSxPQUFPLHFCQUFxQjtJQUNmLElBQUksR0FBRyxJQUFJLEdBQUcsRUFBSyxDQUFBO0lBQzVCLGdCQUFnQixHQUFHLENBQUMsQ0FBQztJQUU3Qjs7OztPQUlHO0lBQ0gsQ0FBRSxPQUFPLENBQUMsTUFBUztRQUNqQixNQUFNLEVBQUUsSUFBSSxFQUFFLEdBQUcsSUFBSSxDQUFBO1FBQ3JCLE1BQU0sWUFBWSxHQUFHLGdCQUFnQixDQUFDLE1BQU0sRUFBRSxJQUFJLENBQUMsQ0FBQTtRQUVuRCxJQUFJLFlBQVksRUFBRTtZQUNoQixJQUFJLENBQUMsTUFBTSxDQUFDLFlBQVksQ0FBQyxDQUFBO1lBQ3pCLElBQUksQ0FBQyxnQkFBZ0IsRUFBRSxDQUFBO1lBQ3ZCLE9BQU07U0FDUDtRQUVELElBQUksTUFBTSxDQUFDLE1BQU0sRUFBRTtZQUNqQixJQUFJLENBQUMsR0FBRyxDQUFDLE1BQU0sQ0FBQyxDQUFBO1lBQ2hCLElBQUksQ0FBQyxnQkFBZ0IsRUFBRSxDQUFBO1lBQ3ZCLE9BQU07U0FDUDtRQUVELElBQUksSUFBSSxDQUFDLGdCQUFnQixHQUFHLENBQUMsRUFBRTtZQUM3QixJQUFJLENBQUMsR0FBRyxDQUFDLE1BQU0sQ0FBQyxDQUFBO1NBQ2pCO2FBQU07WUFDTCxJQUFJLElBQUksQ0FBQyxJQUFJLEVBQUU7Z0JBQ2IsS0FBSyxDQUFDLENBQUMsSUFBSSxDQUFBO2dCQUNYLElBQUksQ0FBQyxLQUFLLEVBQUUsQ0FBQTthQUNiO1lBRUQsTUFBTSxNQUFNLENBQUE7U0FDYjtJQUNILENBQUM7SUFFRDs7O09BR0c7SUFDSCxDQUFFLEtBQUs7UUFDTCxNQUFNLEVBQUUsSUFBSSxFQUFFLEdBQUcsSUFBSSxDQUFBO1FBRXJCLElBQUksSUFBSSxDQUFDLElBQUksRUFBRTtZQUNiLEtBQUssQ0FBQyxDQUFDLElBQUksQ0FBQTtZQUNYLElBQUksQ0FBQyxLQUFLLEVBQUUsQ0FBQTtTQUNiO0lBQ0gsQ0FBQztDQUNGIn0=