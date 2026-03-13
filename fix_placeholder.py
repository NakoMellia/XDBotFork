import os, re

root = r'd:\VSCodeProjects\XDbotNew\xdBot-2.4.0\src'
for dp, dn, fns in os.walk(root):
    for f in fns:
        if not f.endswith(('.cpp', '.hpp')):
            continue
        fpath = os.path.join(dp, f)
        with open(fpath, 'r', encoding='utf-8', errors='ignore') as fh:
            content = fh.read()
        if 'm_placeholderLabel' not in content:
            continue
        new_content = re.sub(r'(?m)^([ \t]*)(.*m_placeholderLabel.*)', r'\1// \2', content)
        with open(fpath, 'w', encoding='utf-8') as fh:
            fh.write(new_content)
        print(f'Fixed: {fpath}')
