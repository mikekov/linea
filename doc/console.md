# Console

The console is an on-canvas command palette for running Linea commands. Open it with `?` or **View → Command Palette**. Commands can create and modify objects, manage documents, run scripts, and invoke registered actions.

Use `;` to run multiple commands on one line. Quote arguments that contain spaces. Optional arguments are shown in square brackets.

| Command | Description |
|---|---|
| `help [command]` | List commands or show help for one command. |
| `actions [filter]` | List registered action IDs, optionally filtered by text. |
| `echo [-e] text...` | Print text; `-e` decodes escape sequences. |
| `new [template-index]` | Create a new document. |
| `open <file>` | Open a document from a file path. |
| `close` | Close the active document window. |
| `revert` | Restore the active document to its saved version. |
| `quit` | Close all documents and exit Linea. |
| `select all\|invert\|none` | Change the current selection. |
| `select "object-id"` | Select the object with the given SVG ID. |
| `location [x y]` | Show or set the default origin for shape commands. |
| `rect ["id"] w [h [x y]]` | Create a rectangle; `h` defaults to `w`. |
| `oval ["id"] r1 [r2 [x y]]` | Create an ellipse centered at `x y`. |
| `circle ["id"] r [x y]` | Create a circle centered at `x y`. |
| `line ["id"] x y [xn yn]+` | Create a line or polyline from coordinate pairs. |
| `polygon ["id"] sides size [x y]` | Create a regular polygon; `size` is side length. |
| `path ["id"] <svg-path-data>` | Create a path from SVG path data. |
| `move dx [dy]` | Move the selection; `dy` defaults to `0`. |
| `scale scale [scale-y]` | Scale the selection about its center. |
| `rotate angle` | Rotate the selection about its center, in degrees. |
| `skew sx [sy]` | Skew the selection about its center. |
| `flip x\|y\|xy` | Mirror the selection horizontally, vertically, or both. |
| `fill none\|inherit\|NN%\|css-color` | Set fill or fill opacity on the selection. |
| `stroke [none\|inherit\|NN%\|css-color] [width\|hairline]` | Set stroke color, opacity, or width. |
| `opacity 0..1\|0%..100%` | Set the selection's opacity. |
| `duplicate` | Duplicate the selected objects. |
| `run [name]` | Run the current or named script. |
| `<action-id>` | Run any registered action; use `actions` to list IDs. |

Example:

```text
rect "tile" 40 24 20 20; fill #4f8cff; stroke "#172033" 2
```

Press `Tab` for command and argument completion. Use `Up` and `Down` for command history, `Esc` to clear the input or close the panel, and `Ctrl+C` to request cancellation.
