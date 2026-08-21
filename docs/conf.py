project = "P2000T Teletekst Cartridge Interface"
author = "Ivo Filot"
copyright = "2026, Ivo Filot"
version = "P2WP/2"
release = "P2WP/2"

extensions = ["myst_parser", "sphinx_rtd_theme"]

source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

root_doc = "index"
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]
nitpicky = True

myst_heading_anchors = 3

html_theme = "sphinx_rtd_theme"
html_title = "P2WP/2 Interface Protocol"
html_show_sourcelink = False

html_theme_options = {
    "collapse_navigation": False,
    "navigation_depth": 4,
    "sticky_navigation": True,
}
