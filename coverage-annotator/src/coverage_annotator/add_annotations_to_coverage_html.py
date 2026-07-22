# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2025 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

import json
from bs4 import BeautifulSoup
import sys

def add_annotations_to_coverage_html(html_path, json_path, output_path):
    # Load annotations from JSON file
    with open(json_path, 'r', encoding='utf-8') as jf:
        annotations = json.load(jf)

    # Parse HTML
    with open(html_path, 'r', encoding='utf-8') as hf:
        soup = BeautifulSoup(hf, 'html.parser')

    # Find the coverage file table
    # Commonly, the file list is inside a <table class="index"> or similar
    table = soup.find('table')
    if not table:
        raise RuntimeError("Could not find a table in the HTML file")

    # Add new column header
    header_row = table.find('tr')
    if header_row:
        new_th = soup.new_tag('th')
        new_th.string = 'Annotation'
        header_row.append(new_th)

    # Iterate over table rows (skip header)
    rows = table.find_all('tr')[1:]
    for row in rows:
        cols = row.find_all(['td', 'th'])
        if not cols:
            continue

        # The first column usually contains the filename
        file_link = cols[0].find('a')
        filename = None
        if file_link and file_link.text:
            filename = file_link.text.strip()
        else:
            filename = cols[0].text.strip()

        annotation_list = annotations.get(filename, [])

        # Convert list to HTML bullet list or plain text
        if annotation_list:
            annotation_html = "<ul>" + "".join(
                f"<li>{note}</li>" for note in annotation_list
            ) + "</ul>"
        else:
            annotation_html = "<i>—</i>"

        # Add new cell
        new_td = soup.new_tag("td")
        new_td["class"] = "column-entry"
        new_td.append(BeautifulSoup(annotation_html, "html.parser"))
        row.append(new_td)

    # Save modified HTML
    with open(output_path, 'w', encoding='utf-8') as out:
        out.write(str(soup))

    print(f"✅ Annotated HTML saved to: {output_path}")


def main():
    if len(sys.argv) != 4:
        print("Usage: uv run add-annotations coverage/html/index.html annotations.json coverage_with_annotations.html>")
        sys.exit(1)

    html_path, json_path, output_path = sys.argv[1:]
    add_annotations_to_coverage_html(html_path, json_path, output_path)
