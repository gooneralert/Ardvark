-- getscripts / bytecode / decompile smoke
local scripts = getscripts()
print("scripts", #scripts)

local shown = 0
for i, s in ipairs(scripts) do
	local cls = s.ClassName
	local bc = getscriptbytecode(s)
	if bc and #bc > 0 then
		shown = shown + 1
		print(string.format("[%d] %s %s bytes=%d head=%q", i, cls, s.Name, #bc, bc:sub(1, 4)))
		if shown <= 2 then
			local src = decompile(s)
			print("--- decompile ---")
			print(src:sub(1, 1200))
			print("--- end ---")
		end
		if shown >= 5 then
			break
		end
	end
end

if shown == 0 then
	print("no scripts with readable bytecode (offsets/game?)")
end
