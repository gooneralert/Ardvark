-- Vector3 / Color3 / Drawing smoke
local a = Vector3.new(1, 2, 3)
local b = Vector3.new(0, 1, 0)
print("v3", a, "mag", a.Magnitude, "dot", a:Dot(b))

local c = Color3.fromRGB(255, 80, 40)
print("color", c)

Drawing.clear()

local line = Drawing.new("Line")
line.From = Vector2.new(40, 40)
line.To = Vector2.new(240, 120)
line.Color = c
line.Thickness = 2
line.Visible = true

local text = Drawing.new("Text")
text.Text = "jewsploit Drawing"
text.Position = Vector2.new(40, 20)
text.Size = 16
text.Color = Color3.new(1, 1, 1)
text.Outline = true
text.Visible = true

local circ = Drawing.new("Circle")
circ.Position = Vector2.new(300, 200)
circ.Radius = 40
circ.Color = Color3.fromRGB(80, 200, 255)
circ.Thickness = 2
circ.Filled = false
circ.Visible = true

local box = Drawing.new("Square")
box.Position = Vector2.new(360, 160)
box.Size = Vector2.new(80, 80)
box.Color = Color3.fromRGB(120, 255, 120)
box.Filled = true
box.Transparency = 0.4
box.Visible = true

print("drawing ok — hold LMB to move line.To (~5s)")
for i = 1, 150 do
	local m = getmousepos()
	if iskeypressed("left") then
		line.To = m
	end
	text.Text = string.format("mouse %d %d  rbx=%s", m.X, m.Y, tostring(isrbxactive()))
	task.wait(1 / 30)
end

print("done — call Drawing.clear() to wipe")
