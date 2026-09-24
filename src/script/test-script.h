// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef LINEA_SCRIPT_TEST_SCRIPT_H
#define LINEA_SCRIPT_TEST_SCRIPT_H

namespace Linea::Script {

inline constexpr char test_script[] = R"lua(-- Linea scripting integration test.
-- Generate a small, repetitive composition in the current document and test
-- object creation, attributes, styles, transforms, lookup, and deletion.

print("script test: started")

local document = linea.document()
assert(document ~= nil, "no active document")

local function create_rect(id, x, y, width, height, fill)
  local rect = document:create_rect(x, y, width, height, {
    fill = fill,
    stroke = "#172033",
    stroke_width = 1.5
  })
  rect.label = "Script tile " .. id
  return rect
end

local function create_circle(id, x, y, radius, fill)
  local circle = document:create_ellipse(x, y, radius, {
    fill = fill,
    stroke = "#172033",
    stroke_width = 1,
    opacity = 0.86
  })
  circle.label = "Script dot " .. id
  return circle
end

-- Inspect the pre-existing selection before adding generated geometry.
local selection = assert(linea.selection(), "no active selection")
local selection_count = selection:count()
local selection_ids = selection:ids()
assert(#selection_ids == selection_count, "selection count mismatch")
print("initial selection count: " .. selection_count)

for index, id in ipairs(selection_ids) do
  local object = document:object(id)
  assert(object ~= nil, "selected object disappeared: " .. id)
  assert(object.id == id, "object identity mismatch: " .. id)
  print("selection[" .. index .. "]: " .. id)
end

local palette = {"#4f8cff", "#70d6a2", "#ffb347", "#ff6b8a", "#b18cff"}
local tile_count = 0

-- Build a repeated, alternating tile field using nested imperative loops.
for row = 0, 3 do
  for column = 0, 5 do
    tile_count = tile_count + 1
    local size = 28 + ((row + column) % 3) * 5
    local x = 30 + column * 48
    local y = 30 + row * 48
    local tile = create_rect(
      "script-tile-" .. tile_count,
      x,
      y,
      size,
      size,
      palette[((tile_count - 1) % #palette) + 1]
    )
    tile:rotate((row * 7 + column * 11) % 36 - 18)
    if (row + column) % 2 == 0 then
      tile:scale(0.82, 1.08)
    else
      tile:translate(4, -3)
    end
  end
end

-- Build a radial rosette from repeated circles with varying radii and offsets.
local center_x, center_y = 190, 270
for petal = 0, 15 do
  local angle = petal * math.pi * 2 / 16
  local radius = 72 + (petal % 3) * 12
  local x = center_x + math.cos(angle) * radius
  local y = center_y + math.sin(angle) * radius
  local dot = create_circle(
    "script-petal-" .. petal,
    x,
    y,
    10 + (petal % 4) * 2,
    palette[(petal % #palette) + 1]
  )
  dot:rotate(petal * 22.5)
  if petal % 2 == 0 then
    dot:shear(0.12, 0)
  end
end

local ellipse = document:create_ellipse(390, 310, 36, 18, {
  fill = "#70d6a2",
  stroke = "#172033"
})
assert(ellipse.style.fill == "#70d6a2", "ellipse style table was not applied")
ellipse.label = "Script ellipse"

local polygon = document:create_polygon(455, 300, 6, 24, {
  fill = "#b18cff",
  stroke = "#321b38"
})
assert(polygon:get_attribute("sodipodi:type") == "star", "polygon type mismatch")
assert(polygon:get_attribute("inkscape:flatsided") == "true", "polygon is not flat-sided")
assert(polygon:get_attribute("sodipodi:sides") == "6", "polygon side count mismatch")
polygon.label = "Script polygon"

local star = document:create_star(535, 300, 5, 36, 14, {
  fill = "#ffb347",
  stroke = "#8f4f20"
})
assert(star:get_attribute("sodipodi:type") == "star", "star type mismatch")
assert(star:get_attribute("inkscape:flatsided") == "false", "star unexpectedly flat-sided")
assert(star:get_attribute("sodipodi:sides") == "5", "star side count mismatch")
star.label = "Script star"

local default_star = document:create_star(600, 300, 7, 20, {
  fill = "#ffd166"
})
assert(default_star:get_attribute("sodipodi:sides") == "7", "default star side count mismatch")
assert(math.abs(tonumber(default_star:get_attribute("sodipodi:r2")) - 10) < 1e-6,
       "default star inner radius mismatch")
assert(default_star.style.fill == "#ffd166", "default star style table was not applied")

local text = document:create_text(80, 430, "First line\nSecond line", {
  font_family = "sans-serif",
  font_size = 18,
  fill = "#172033"
})
assert(text.text == "First line\nSecond line", "created text content mismatch")
text.text = "Updated line"
assert(text.text == "Updated line", "updated text content mismatch")
text.label = "Script text"

-- Exercise deletion independently of the generated composition.
local temporary = create_rect("script-temporary", 0, 0, 5, 5, "#ffffff")
local temporary_id = temporary.id
local temporary_parent = temporary.parent
assert(temporary_parent ~= nil, "created object has no parent")
local found_temporary = false
for _, child in ipairs(temporary_parent.children) do
  if child.id == temporary_id then
    found_temporary = true
    break
  end
end
assert(found_temporary, "created object missing from parent children")
temporary:delete()
assert(document:object(temporary_id) == nil, "delete did not remove temporary object")

local path = document:create_path({
  fill = "none",
  stroke = "#111111",
  stroke_width = 2
})
assert(path.style.stroke == "#111111", "path style table was not applied")
path:move_to(300, 40)
path:line_to(390, 40)
path:vertical_by(45)
path:horizontal_by(-35)
path:cubic_by(20, 35, 55, 35, 75, 0)
path:smooth_cubic_by(35, -35, 60, 0)
path:quadratic_by(-20, 30, -55, 0)
path:smooth_quadratic_by(-35, -30)
path:arc_by(18, 18, 0, 0, 1, -30, -25)
path:close()
path.style.fill = "#d0bcff"
path.style.stroke = "#53389e"
path.style.stroke_width = 2
print("created path with lines, cubics, quadratics, arc, and close")

local missing = document:object("__linea_script_test_missing_object__")
assert(missing == nil, "missing object lookup did not return nil")

print("generated tiles: " .. tile_count)
print("generated petals: 16")
print("script test: passed")
)lua";

inline constexpr char flower_script[] = R"lua(-- Parametric flower generator.
-- Creates layered petals around a center using polar coordinates.

local document = assert(linea.document(), "no active document")
local palette = {"#ff4f81", "#ffb347", "#70d6a2", "#4f8cff", "#b18cff"}
local cx, cy = 260, 250
local petals = 36

local function create_petal(index, ring, angle)
  local radius = 42 + ring * 34
  local x = cx + math.cos(angle) * radius
  local y = cy + math.sin(angle) * radius
  local petal = document:create_ellipse(x, y, 13 + ring * 2, 38 - ring * 3)
  petal.style.fill = palette[((index + ring) % #palette) + 1]
  petal.style.stroke = "#321b38"
  petal.style.stroke_width = 1.5
  petal.style.opacity = 0.72 - ring * 0.08
  petal.label = "Flower petal " .. ring .. ":" .. index
  petal:rotate(angle * 180 / math.pi + 90)
  if index % 3 == 0 then
    petal:shear(0.12, 0)
  end
end

for ring = 0, 2 do
  for index = 0, petals - 1 do
    local angle = index * math.pi * 2 / petals + ring * 0.08
    create_petal(index, ring, angle)
  end
end

local center = document:create_ellipse(cx, cy, 30)
center.style.fill = "#ffd166"
center.style.stroke = "#8f4f20"
center.style.stroke_width = 3
center.label = "Flower center"

for index = 0, 11 do
  local angle = index * math.pi * 2 / 12
  local dot = document:create_ellipse(
    cx + math.cos(angle) * 17,
    cy + math.sin(angle) * 17,
    4 + index % 3
  )
  dot.style.fill = "#8f4f20"
  dot.style.stroke = "none"
end

print("flower generated: 108 petals and 12 seeds")
)lua";

inline constexpr char mandala_script[] = R"lua(-- Parametric mandala generator.
-- Builds concentric rings of alternating geometric motifs.

local document = assert(linea.document(), "no active document")
local palette = {"#101828", "#344054", "#7f56d9", "#12b76a", "#f79009", "#f04438"}
local cx, cy = 270, 270
local ring_count = 7
local total = 0

local function paint(object, fill, opacity, stroke_width)
  object.style.fill = fill
  object.style.stroke = "#ffffff"
  object.style.stroke_width = stroke_width
  object.style.opacity = opacity
end

for ring = 1, ring_count do
  local count = ring * 8
  local radius = ring * 31
  for index = 0, count - 1 do
    total = total + 1
    local angle = index * math.pi * 2 / count + ring * 0.11
    local x = cx + math.cos(angle) * radius
    local y = cy + math.sin(angle) * radius
    local fill = palette[((ring + index) % #palette) + 1]

    if ring % 2 == 0 then
      local square = document:create_rect(x - 10, y - 10, 20 + ring, 20 + ring)
      paint(square, fill, 0.82, 1.2)
      square:rotate(angle * 180 / math.pi + 45)
      square:scale(0.75 + ring * 0.04)
    else
      local dot = document:create_ellipse(x, y, 7 + ring * 0.9)
      paint(dot, fill, 0.88, 1.1)
      dot:shear((index % 3) * 0.08, 0)
    end
  end
end

local center = document:create_ellipse(cx, cy, 24)
paint(center, "#fef0c7", 1, 2.5)
center.label = "Mandala center"

print("mandala generated: " .. total .. " motifs")
)lua";

} // namespace Linea::Script

#endif // LINEA_SCRIPT_TEST_SCRIPT_H
