from sphinx.application import Sphinx
from pathlib import Path
import json
import os
import re

if not os.environ.get('LIBHAL_API_VERSION'):
    print("\nEnvironment variable 'LIBHAL_API_VERSION' must be set!")
    exit(1)

API_VERSION = os.environ.get('LIBHAL_API_VERSION')

doxygen_conf = Path('doxygen.conf').read_text()
match = re.search(r'PROJECT_NAME\s*=\s*"([^"]+)"', doxygen_conf)
if not match:
    print("\nCould not find PROJECT_NAME in doxygen.conf!")
    exit(1)
LIBRARY_NAME = match.group(1)

DEFAULT_SWITCHER_URL = f"https://libhal.github.io/api/{LIBRARY_NAME}/switcher.json"

LOCAL_BUILD = os.environ.get('LIBHAL_LOCAL_BUILD')

if LOCAL_BUILD:
    switcher_url = "switcher.json"
else:
    switcher_url = DEFAULT_SWITCHER_URL

html_theme = 'pydata_sphinx_theme'
html_theme_options = {
    "navbar_start": ["navbar-logo", "guide_links", "version-switcher"],
    "navbar_center": ["navbar-nav"],
    "navbar_end": ["navbar-icon-links"],
    "navbar_persistent": ["search-button-field", "theme-switcher"],
    "header_links_before_dropdown": 3,
    "switcher": {
        "json_url": switcher_url,
        "version_match": API_VERSION,
    },
    "check_switcher": False,
}

extensions = ["breathe", "myst_parser"]


def write_local_switcher(app, exception):
    if LOCAL_BUILD and exception is None:
        outdir = Path(app.outdir)
        (outdir / 'switcher.json').write_text(
            json.dumps([{"version": API_VERSION, "url": "."}]))


def setup(app: Sphinx):
    app.add_css_file("extra.css")
    app.connect("build-finished", write_local_switcher)


html_css_files = [
    'extra.css',
]
breathe_projects = {LIBRARY_NAME: "build/xml"}
breathe_default_project = LIBRARY_NAME
breathe_default_members = ('members',)
project = LIBRARY_NAME
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown'
}
master_doc = 'index'
exclude_patterns = ['.venv', 'build']
highlight_language = 'c++'
html_static_path = ['_static']
templates_path = ['_templates']
todo_include_todos = False
html_favicon = "_static/favicon.ico"
html_logo = "_static/logo.png"
