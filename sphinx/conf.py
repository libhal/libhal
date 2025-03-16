from sphinx.application import Sphinx
import os

html_theme = 'pydata_sphinx_theme'
html_theme_options = {
    "navbar_start": ["navbar-logo", "guide_links"],
    "navbar_center": ["navbar-nav"],
    "navbar_end": ["navbar-icon-links"],
    "navbar_persistent": ["search-button-field", "theme-switcher"],
    "header_links_before_dropdown": 3
}

extensions = ["breathe", "myst_parser"]


def generate_back_uri():
    if not os.environ.get('PRODUCTION') or not os.environ.get('CURRENT_VERSION'):
        return "/"

    uri = "/" + os.environ['CURRENT_VERSION'] + "/"
    return uri


def setup(app: Sphinx):
    app.add_css_file("extra.css")
    app.connect('builder-inited', inject_custom_function)


def inject_custom_function(app: Sphinx):
    app.builder.templates.environment.globals['generate_back_uri'] = generate_back_uri


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
