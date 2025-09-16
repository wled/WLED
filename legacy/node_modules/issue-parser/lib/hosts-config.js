module.exports = {
  github: {
    actions: {
      // https://help.github.com/articles/closing-issues-using-keywords
      close: ['close', 'closes', 'closed', 'fix', 'fixes', 'fixed', 'resolve', 'resolves', 'resolved'],
      block: [],
      require: [],
      parentOf: [],
      childOf: [],
      // https://help.github.com/articles/about-duplicate-issues-and-pull-requests
      duplicate: ['Duplicate of'],
    },
    delimiters: [':'],
    // https://guides.github.com/features/issues/#notifications
    mentionsPrefixes: ['@'],
    issuePrefixes: ['#', 'gh-'],
    hosts: ['https://github.com'],
    issueURLSegments: ['issues', 'pull'],
  },
  bitbucket: {
    actions: {
      // https://confluence.atlassian.com/bitbucket/resolve-issues-automatically-when-users-push-code-221451126.html
      close: [
        'close',
        'closes',
        'closed',
        'closing',
        'fix',
        'fixes',
        'fixed',
        'fixing',
        'resolve',
        'resolves',
        'resolved',
        'resolving',
      ],
      block: [],
      require: [],
      parentOf: [],
      childOf: [],
      duplicate: [],
    },
    delimiters: [],
    // https://confluence.atlassian.com/bitbucket/mark-up-comments-issues-and-commit-messages-321859781.html
    mentionsPrefixes: ['@'],
    // https://confluence.atlassian.com/bitbucket/mark-up-comments-issues-and-commit-messages-321859781.html
    issuePrefixes: ['#'],
    hosts: [],
    issueURLSegments: [],
  },
  gitlab: {
    actions: {
      // https://docs.gitlab.com/ee/user/project/issues/automatic_issue_closing.html
      close: [
        'close',
        'closes',
        'closed',
        'closing',
        'fix',
        'fixes',
        'fixed',
        'fixing',
        'resolve',
        'resolves',
        'resolved',
        'resolving',
        'implement',
        'implements',
        'implemented',
        'implementing',
      ],
      block: [],
      require: [],
      parentOf: [],
      childOf: [],
      // https://gitlab.com/gitlab-org/gitlab-ce/merge_requests/12845
      duplicate: ['/duplicate'],
    },
    delimiters: [],
    // https://about.gitlab.com/2016/03/08/gitlab-tutorial-its-all-connected
    mentionsPrefixes: ['@'],
    // https://about.gitlab.com/2016/03/08/gitlab-tutorial-its-all-connected
    issuePrefixes: ['#', '!'],
    hosts: ['https://gitlab.com'],
    issueURLSegments: ['issues', 'merge_requests'],
  },
  default: {
    actions: {
      close: [
        'close',
        'closes',
        'closed',
        'closing',
        'fix',
        'fixes',
        'fixed',
        'fixing',
        'resolve',
        'resolves',
        'resolved',
        'resolving',
        'implement',
        'implements',
        'implemented',
        'implementing',
      ],
      duplicate: ['Duplicate of', '/duplicate'],
    },
    delimiters: [':'],
    mentionsPrefixes: ['@'],
    issuePrefixes: ['#', 'gh-'],
    hosts: ['https://github.com', 'https://gitlab.com'],
    issueURLSegments: ['issues', 'pull', 'merge_requests'],
  },
};
