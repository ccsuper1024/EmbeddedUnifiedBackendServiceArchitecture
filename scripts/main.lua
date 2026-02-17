local rtp_forward_udp_session = nil
local rtp_forward_by_ssrc = {}

function lua_on_tcp_message(event)
    cpp_send_tcp(event.session_id, event.payload)
end

function lua_register_rtp_forward(ssrc, udp_session_id)
    rtp_forward_by_ssrc[ssrc] = udp_session_id
end

function lua_on_udp_signal(event)
    if event.payload == "register_rtp_forward" then
        rtp_forward_udp_session = event.session_id
        cpp_log("info", "set rtp forward udp_session=" .. tostring(event.session_id))
        return
    end
    local prefix = "register_rtp_forward_ssrc "
    if #event.payload > #prefix and string.sub(event.payload, 1, #prefix) == prefix then
        local ssrc_text = string.sub(event.payload, #prefix + 1)
        local ssrc = tonumber(ssrc_text)
        if ssrc ~= nil then
            lua_register_rtp_forward(ssrc, event.session_id)
            cpp_log("info", "set rtp forward for ssrc=" .. tostring(ssrc)
                .. " udp_session=" .. tostring(event.session_id))
        end
        return
    end
    cpp_send_udp(event.session_id, event.payload)
end

function lua_on_rtp(event)
    local target = rtp_forward_by_ssrc[event.session_id]
    if target == nil then
        target = rtp_forward_udp_session
    end
    if target ~= nil then
        cpp_send_udp(target, event.payload)
    end
    cpp_log("info", "rtp bytes=" .. tostring(#event.payload)
        .. " ssrc_id=" .. tostring(event.session_id)
        .. " from=" .. tostring(event.remote_ip) .. ":" .. tostring(event.remote_port))
end

function lua_on_timer(event)
end

function lua_on_disk_done(event)
end
