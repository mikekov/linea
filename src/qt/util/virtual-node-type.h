#pragma once

namespace Linea::UI {

/**
 * Types of virtual nodes that don't correspond to actual SPObjects.
 */
enum class VirtualNodeType {
    None,           // Not a virtual node - backed by SPObject
    DocumentProps,  // "Document properties" root virtual node
    Display,        // Display settings from namedview
    Pages,          // Pages collection
    Swatches,       // Swatch definitions (CSS vars) from global <style> elements
    Styles,         // Styles collection; all reusable CSS styles
    About,          // Combined title + metadata
    Canvas,         // Canvas size from root viewBox
    Grids,          // Grids collection
    Guides,         // Guides collection
    Symbols,        // Symbols collection in the document
    Resources,      // Document resources (show statistics)
};

} // namespace Linea::UI
