# Scripting

Linea scripts are written in Lua and run against the active document. Use the Script panel to edit and run named scripts, or run the current/named script from the console with `run [name]`. `print()` writes to the script output area or console.

Scripts can create SVG elements, inspect and change selections, modify object attributes and styles, transform objects, and append SVG path commands. Changes made by a successful run are grouped into one undo step.

## Global functions

| Function | Description |
|---|---|
| `linea.document()` | Return the active document, or `nil`. |
| `linea.document(index)` | Return the nth loaded document, or `nil` if the index is out of range. |
| `linea.selection()` | Return the active selection, or `nil`. |
| `linea.run_action("action-id")` | Run a registered Linea action. |
| `print(...)` | Write output for the script. |

## Document methods

| Method | Description |
|---|---|
| `document:object("id")` | Return the object with an SVG ID, or `nil`. |
| `document:create("tag", attributes)` | Create an SVG element in the current layer. Attributes may be strings, numbers, or booleans. |
| `document:create_rect(x, y, width, height[, style])` | Create a rectangle in the current layer. |
| `document:create_ellipse(x, y, r1[, r2][, style])` | Create an ellipse centered at `x y`; without `r2`, creates a circle. |
| `document:create_polygon(x, y, sides, side_length[, style])` | Create a regular flat-sided polygon centered at `x y`; `sides` is 3–1024. |
| `document:create_star(x, y, corners, r1[, r2][, style])` | Create a star centered at `x y`; `r2` defaults to `r1 * 0.5`. |
| `document:create_text(x, y, content[, style])` | Create text at baseline `x y`. |
| `document:create_path([style])` | Create an empty SVG path object. |

`document:create()` accepts an optional `id`. If omitted, an ID is generated; if supplied, it is made unique when necessary. Dedicated `create_*` methods accept an optional CSS property table as their final argument. For `create_ellipse()` and `create_star()`, a table in the optional radius position selects the default radius and applies the table as the style.

## Selection methods

| Method | Description |
|---|---|
| `selection:count()` | Return the number of selected objects. |
| `selection:ids()` | Return an array of selected object IDs. |
| `selection:items()` | Return an array of selected objects. |
| `selection:set(object)` | Replace the selection with one object. |
| `selection:add(object)` | Add an object to the selection. |
| `selection:remove(object)` | Remove an object from the selection. |
| `selection:clear()` | Clear the selection. |

## Object fields

| Field | Description |
|---|---|
| `object.id` | Read-only SVG ID, or `nil`. |
| `object.label` | Read/write `inkscape:label` value. |
| `object.text` | Read/write text content; valid only for text objects. Newlines map to text line spans. |
| `object.parent` | Read-only parent object, or `nil`. |
| `object.children` | Read-only array of direct child objects. |
| `object.style` | Read-only style proxy; use it to read and write CSS properties. |

## Object methods

| Method | Description |
|---|---|
| `object:get_attribute("name")` | Return an SVG attribute value, or `nil`. |
| `object:set_attribute("name", "value")` | Set an SVG attribute from a string. |
| `object:set_style("name", value)` | Set an inline CSS property; prefer `object.style`. |
| `object:delete()` | Delete the object from the document. |
| `object:translate(dx, dy)` | Move the object by document coordinates. |
| `object:scale(sx[, sy])` | Scale about the object's center; `sy` defaults to `sx`. |
| `object:rotate(degrees)` | Rotate about the object's center. |
| `object:shear(sx[, sy])` | Skew about the object's center; `sy` defaults to `0`. |

## Style fields

Use `object.style.<name>` to read or write an inline CSS property. Underscores are converted to hyphens, so `stroke_width` maps to `stroke-width`. Values may be strings or numbers.

```lua
object.style.fill = "#4f8cff"
object.style.stroke_width = 2
print(object.style.opacity)
```

## Path methods

Path methods append SVG commands to an existing path object.

| Method | Description |
|---|---|
| `path:move_to(x, y)`, `path:move_by(dx, dy)` | Append absolute or relative `M` commands. |
| `path:line_to(x, y)`, `path:line_by(dx, dy)` | Append absolute or relative `L` commands. |
| `path:horizontal_to(x)`, `path:horizontal_by(dx)` | Append `H` or `h` commands. |
| `path:vertical_to(y)`, `path:vertical_by(dy)` | Append `V` or `v` commands. |
| `path:cubic_to(...)`, `path:cubic_by(...)` | Append six-argument `C` or `c` commands. |
| `path:smooth_cubic_to(...)`, `path:smooth_cubic_by(...)` | Append four-argument `S` or `s` commands. |
| `path:quadratic_to(...)`, `path:quadratic_by(...)` | Append four-argument `Q` or `q` commands. |
| `path:smooth_quadratic_to(x, y)`, `path:smooth_quadratic_by(dx, dy)` | Append `T` or `t` commands. |
| `path:arc_to(...)`, `path:arc_by(...)` | Append seven-argument `A` or `a` commands. |
| `path:close()` | Append `Z` to close the path. |

## Examples

### Create and style an element

```lua
local document = assert(linea.document(), "no active document")

local rect = document:create_rect(40, 40, 120, 80, {
  fill = "#4f8cff",
  stroke = "#172033",
  stroke_width = 2
})

rect.label = "Generated rectangle"

local hexagon = document:create_polygon(260, 80, 6, 35)
hexagon.style.fill = "#70d6a2"

local text = document:create_text(40, 180, "Hello\nWorld", {
  font_family = "sans-serif",
  font_size = 18,
  fill = "#172033"
})
text.text = "Updated text"
```

### Inspect the current selection

```lua
local selection = assert(linea.selection(), "no active selection")
local document = assert(linea.document(), "no active document")

print("selected objects: " .. selection:count())

for _, id in ipairs(selection:ids()) do
  local object = document:object(id)
  if object then
    print(id .. " label: " .. tostring(object.label))
  end
end
```

### Modify selected objects

```lua
local selection = assert(linea.selection(), "no active selection")

for _, object in ipairs(selection:items()) do
  object.label = "Processed " .. tostring(object.id)
  object.style.opacity = 0.8
  object:translate(10, 0)
end
```

### Create a path

```lua
local document = assert(linea.document(), "no active document")
local path = document:create_path()

path:move_to(50, 50)
path:line_to(150, 50)
path:cubic_by(20, 30, 60, 30, 80, 0)
path:arc_by(20, 20, 0, 0, 1, 30, -20)
path:close()

path.style.fill = "none"
path.style.stroke = "#53389e"
path.style.stroke_width = 2
```
