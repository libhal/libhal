from sphinx.application import Sphinx
import os

if not os.environ.get('LIBHAL_API_VERSION'):
    print("\nEnvironment variable 'LIBHAL_API_VERSION' must be set!")
    exit(1)

html_theme = 'pydata_sphinx_theme'
html_theme_options = {
    "navbar_start": ["navbar-logo", "guide_links", "version-switcher"],
    "navbar_center": ["navbar-nav"],
    "navbar_end": ["navbar-icon-links"],
    "navbar_persistent": ["search-button-field", "theme-switcher"],
    "header_links_before_dropdown": 3,
    "switcher": {
        "json_url": "https://libhal.github.io/libhal/switcher.json",
        "version_match": os.environ.get('LIBHAL_API_VERSION'),
    },
    "check_switcher": False,
}

extensions = ["breathe", "myst_parser"]


def setup(app: Sphinx):
    app.add_css_file("extra.css")


html_css_files = [
    'extra.css',
]
breathe_projects = {"libhal": "doxygen/xml"}
breathe_default_project = "libhal"
breathe_default_members = ('members',)
project = "libhal"
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown'
}
master_doc = 'index'
highlight_language = 'c++'
html_static_path = ['_static']
templates_path = ['_templates']
todo_include_todos = False
html_favicon = "_static/favicon.ico"
html_logo = "_static/logo.png"
