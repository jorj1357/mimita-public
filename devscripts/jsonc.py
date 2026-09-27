"""Small JSON-with-comments reader for repository validation scripts."""

import json


def strip_comments(text: str) -> str:
    """Remove // and /* */ comments without touching quoted strings."""
    output = []
    index = 0
    in_string = False
    escaped = False
    length = len(text)

    while index < length:
        char = text[index]
        next_char = text[index + 1] if index + 1 < length else ""

        if in_string:
            output.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            index += 1
            continue

        if char == '"':
            in_string = True
            output.append(char)
            index += 1
        elif char == "/" and next_char == "/":
            index += 2
            while index < length and text[index] not in "\r\n":
                index += 1
        elif char == "/" and next_char == "*":
            end = text.find("*/", index + 2)
            if end < 0:
                raise ValueError("unterminated /* */ comment")
            output.append("\n" * text[index:end + 2].count("\n"))
            index = end + 2
        else:
            output.append(char)
            index += 1

    return "".join(output)


def load(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.loads(strip_comments(handle.read()))
