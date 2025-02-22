with open("client.html", 'r') as fp:
    data = fp.read()

with open("src/web.h", 'w') as fp:
    fp.write(f"const char *HTML_CONTENT = R\"=====(\n{data}\n)=====\";")