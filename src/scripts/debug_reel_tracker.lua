--=============================================================================
-- Fisch reel-bar POSITION TRACKER (debug)
-- Resolves PlayerGui -> reel -> bar -> {playerbar, fish} exactly the way the
-- autofarmer does, prints AbsolutePosition/AbsoluteSize when they change, and
-- draws live on-screen boxes so you can SEE what the luavm can detect.
--
-- Run this on its own (not together with the main fish script). Enter the
-- fishing minigame and watch the boxes/console.
--=============================================================================

local Players = game:GetService("Players")

-- On-screen boxes + text overlay (Drawing API, documented)
local barBox  = Drawing.new("Square"); barBox.Thickness = 2; barBox.Filled = false
barBox.Color = Color3.fromRGB(80, 255, 120); barBox.Visible = true; barBox.ZIndex = 50
local fishBox = Drawing.new("Square"); fishBox.Thickness = 2; fishBox.Filled = false
fishBox.Color = Color3.fromRGB(255, 70, 70); fishBox.Visible = true; fishBox.ZIndex = 50
local info = Drawing.new("Text")
info.Text = "waiting for LocalPlayer..."; info.Color = Color3.fromRGB(255,255,255)
info.Size = 13; info.Font = Drawing.Fonts.Monospace
info.Position = Vector2.new(8, 8); info.Visible = true; info.ZIndex = 60

local function getPlayerGui(p)
	return p and p:FindFirstChildOfClass("PlayerGui")
end

local function probe()
	local lp         = Players.LocalPlayer
	local pg         = getPlayerGui(lp)
	local reel       = pg and pg:FindFirstChild("reel")
	local bar        = reel and reel:FindFirstChild("bar")
	local playerbar  = bar and bar:FindFirstChild("playerbar")
	local fish       = bar and bar:FindFirstChild("fish")
	return { lp=lp, pg=pg, reel=reel, bar=bar, playerbar=playerbar, fish=fish }
end

local lastSig = ""
local lastPos = { x=-1, y=-1, f=-1 }
local lastSize = { x=-1, y=-1, f=-1 }
local lastPrintAt = 0

local function setBox(box, pos, size, visible)
	if box then
		box.Visible = visible and not not pos
		if pos and size then
			box.Position = Vector2.new(pos.X, pos.Y)
			box.Size = Vector2.new(size.X > 0 and size.X or 1, size.Y > 0 and size.Y or 1)
		end
	end
end

task.spawn(function()
	while true do
		local c = probe()

		local pbPos, pbSize = nil, nil
		local fPos, fSize = nil, nil
		local bPos, bSize = nil, nil
		if c.bar        then pcall(function() bPos = c.bar.AbsolutePosition;       bSize = c.bar.AbsoluteSize       end) end
		if c.playerbar  then pcall(function() pbPos = c.playerbar.AbsolutePosition; pbSize = c.playerbar.AbsoluteSize end) end
		if c.fish       then pcall(function() fPos  = c.fish.AbsolutePosition;      fSize  = c.fish.AbsoluteSize      end) end

		-- Draw boxes
		setBox(barBox,  pbPos, pbSize, c.playerbar ~= nil)
		setBox(fishBox, fPos,  fSize,  c.fish ~= nil)

		-- Which link of the chain is null?
		local where = c.lp and ("LP ok" ) or "LP=nil"
		where = where.." | "..(c.pg   and "pg ok"  or "pg=nil")
		where = where.." | "..(c.reel and "reel ok" or "reel=nil")
		where = where.." | "..(c.bar  and "bar ok"  or "bar=nil")
		where = where.." | "..(c.playerbar and "pb ok" or "pb=nil")
		where = where.." | "..(c.fish and "fish ok" or "fish=nil")

		local rbx = false
		pcall(function() rbx = isrbxactive() end)

		info.Text = string.format("FISCH DEBUG | %s | rbx=%s\n"..
			"bar       pos=(%7.1f,%7.1f) size=(%5.1fx%5.1f)\n"..
			"playerbar pos=(%7.1f,%7.1f) size=(%5.1fx%5.1f)\n"..
			"fish      pos=(%7.1f,%7.1f) size=(%5.1fx%5.1f)",
			where, tostring(rbx),
			bPos  and bPos.X  or -1, bPos  and bPos.Y  or -1, bSize  and bSize.X  or -1, bSize  and bSize.Y  or -1,
			pbPos and pbPos.X or -1, pbPos and pbPos.Y or -1, pbSize and pbSize.X or -1, pbSize and pbSize.Y or -1,
			fPos  and fPos.X  or -1, fPos  and fPos.Y  or -1, fSize  and fSize.X  or -1, fSize  and fSize.Y  or -1
		)

		-- Print to console only when values change, throttled to every 0.05s
		local tNow = tick()
		if pbPos and (lastPos.x ~= pbPos.X or lastPos.y ~= pbPos.Y or lastSize.x ~= (pbSize and pbSize.X)) and (tNow - lastPrintAt) >= 0.05 then
			print(string.format("[DEBUG] %s playerbar=(%.1f,%.1f) size=(%.1fx%.1f) fish=(%.1f,%.1f) size=(%.1fx%.1f)",
				where,
				pbPos.X, pbPos.Y, pbSize and pbSize.X or -1, pbSize and pbSize.Y or -1,
				fPos and fPos.X or -1, fPos and fPos.Y or -1, fSize and fSize.X or -1, fSize and fSize.Y or -1))
			lastPos.x = pbPos.X; lastPos.y = pbPos.Y
			lastSize.x = pbSize and pbSize.X or 0
			lastPrintAt = tNow
		end

		task.wait(1/30)
	end
end)

print("[DEBUG] Fisch bar tracker running. Enter the fishing minigame.")
