from HTMLparser import HTMLParser
from collections import deque

class HTMLPatchingParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.lineOffsets = []
        self.changes = deque()

    def feed(self, text):
        self.text = text
        lines = text.split("\n")
        offset = 0
        for i in range(0,len(lines)):
            self.lineOffsets.append(offset)
            offset += len(lines[i]) + 1
        super().feed(text)

    def getOffset(self):
        (line,pos) = self.getpos()
        return self.lineOffsets[line-1] + pos

    def addChange(self,change):
        self.changes.appendleft(change)

    def applyChanges(self):
        t = self.text
        for x in list(self.changes):
            if x["action"] == "insert":
                atOffset = x["at"]
                t = t[0:atOffset] + x["text"] + t[atOffset:]
            if x["action"] == "replace":
                p1 = x["from"]
                p2 = x["to"] 
                t = t[0:p1] + x["text"] + t[p2:]
        return t
