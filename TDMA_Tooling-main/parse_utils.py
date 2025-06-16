def parse_file(file, keys) -> list:
    with open(file, "r") as file:
        data = file.read()
        raw = data.split('\n')
        parsed = []
    for item in raw:
        parsed.append(item.split(","))
    parsed = [dict(zip(keys, item)) for item in parsed]
    return parsed
