var List = require('../utils/list');
var usageUtils = require('./usage');
var clean = require('./clean');
var compress = require('./compress');
var restructureBlock = require('./restructure');
var walkRules = require('../utils/walk').rules;

function readBlock(stylesheet) {
    var buffer = new List();
    var nonSpaceTokenInBuffer = false;
    var protectedComment;

    stylesheet.rules.nextUntil(stylesheet.rules.head, function(node, item, list) {
        if (node.type === 'Comment' && node.value.charAt(0) === '!') {
            if (nonSpaceTokenInBuffer || protectedComment) {
                return true;
            }

            list.remove(item);
            protectedComment = node;
            return;
        }

        if (node.type !== 'Space') {
            nonSpaceTokenInBuffer = true;
        }

        buffer.insert(list.remove(item));
    });

    return {
        comment: protectedComment,
        stylesheet: {
            type: 'StyleSheet',
            rules: buffer
        }
    };
}

function compressBlock(ast, usageData, num, logger) {
    logger('Compress block #' + num, null, true);

    var seed = 1;
    ast.firstAtrulesAllowed = ast.firstAtrulesAllowed;
    walkRules(ast, function() {
        if (!this.stylesheet.id) {
            this.stylesheet.id = seed++;
        }
    });
    logger('init', ast);

    // remove redundant
    clean(ast, usageData);
    logger('clean', ast);

    // compress nodes
    compress(ast, usageData);
    logger('compress', ast);

    return ast;
}

module.exports = function compress(ast, options) {
    options = options || {};
    ast = ast || { type: 'StyleSheet', rules: new List() };

    var logger = typeof options.logger === 'function' ? options.logger : Function();
    var restructuring =
        'restructure' in options ? options.restructure :
        'restructuring' in options ? options.restructuring :
        true;
    var result = new List();
    var block;
    var firstAtrulesAllowed = true;
    var blockNum = 1;
    var blockRules;
    var blockMode = false;
    var usageData = false;
    var info = ast.info || null;

    if (ast.type !== 'StyleSheet') {
        blockMode = true;
        ast = {
            type: 'StyleSheet',
            rules: new List([{
                type: 'Ruleset',
                selector: {
                    type: 'Selector',
                    selectors: new List([{
                        type: 'SimpleSelector',
                        sequence: new List([{
                            type: 'Identifier',
                            name: 'x'
                        }])
                    }])
                },
                block: ast
            }])
        };
    }

    if (options.usage) {
        usageData = usageUtils.buildIndex(options.usage);
    }

    do {
        block = readBlock(ast);
        // console.log(JSON.stringify(block.stylesheet, null, 2));
        block.stylesheet.firstAtrulesAllowed = firstAtrulesAllowed;
        block.stylesheet = compressBlock(block.stylesheet, usageData, blockNum++, logger);

        // structure optimisations
        if (restructuring) {
            restructureBlock(block.stylesheet, usageData, logger);
        }

        blockRules = block.stylesheet.rules;

        if (block.comment) {
            // add \n before comment if there is another content in result
            if (!result.isEmpty()) {
                result.insert(List.createItem({
                    type: 'Raw',
                    value: '\n'
                }));
            }

            result.insert(List.createItem(block.comment));

            // add \n after comment if block is not empty
            if (!blockRules.isEmpty()) {
                result.insert(List.createItem({
                    type: 'Raw',
                    value: '\n'
                }));
            }
        }

        if (firstAtrulesAllowed && !blockRules.isEmpty()) {
            var lastRule = blockRules.last();

            if (lastRule.type !== 'Atrule' ||
               (lastRule.name !== 'import' && lastRule.name !== 'charset')) {
                firstAtrulesAllowed = false;
            }
        }

        result.appendList(blockRules);
    } while (!ast.rules.isEmpty());

    if (blockMode) {
        result = !result.isEmpty() ? result.first().block : {
            type: 'Block',
            info: info,
            declarations: new List()
        };
    } else {
        result = {
            type: 'StyleSheet',
            info: info,
            rules: result
        };
    }

    return {
        ast: result
    };
};
