local serviceId

function OnInit(id)
    print("[lua] ping OnInit id: " .. id)
    serviceId = id
end

function OnServiceMsg(source, buff)
    local n1 = 0
    local n2 = 0

    if buff ~= "start" then
        n1, n2 = string.unpack("i4 i4", buff)
    end

    print("[lua] ping OnServiceMsg n1: " .. n1 .. " n2: " .. n2)
    n1 = n1 + 1
    n2 = n2 + 2

    buff = string.pack("i4 i4", n1, n2)
    sunnet.Send(serviceId, source, buff)

    print("[lua] ping OnServiceMsg id, buff: " .. serviceId , buff)

    if string.len(buff) > 50 then
        sunnet.KillService(serviceId)
        return
    end

    sunnet.Send(serviceId, source, buff .. "i")
end

function OnExit()
    print("[lua] ping OnExit")
end