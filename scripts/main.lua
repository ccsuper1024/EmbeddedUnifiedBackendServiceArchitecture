local rtp_forward_udp_session = nil

function lua_on_tcp_message(event)
    cpp_send_tcp(event.session_id, event.payload)
end

function lua_on_udp_signal(event)
    if event.payload == "register_rtp_forward" then
        rtp_forward_udp_session = event.session_id
        cpp_log("info", "set rtp forward udp_session=" .. tostring(event.session_id))
        return
    end
    cpp_send_udp(event.session_id, event.payload)
end

function lua_on_rtp(event)
    if rtp_forward_udp_session ~= nil then
        cpp_send_udp(rtp_forward_udp_session, event.payload)
    end
    cpp_log("info", "rtp bytes=" .. tostring(#event.payload)
        .. " ssrc_id=" .. tostring(event.session_id)
        .. " from=" .. tostring(event.remote_ip) .. ":" .. tostring(event.remote_port))
end

function lua_on_timer(event)
end

function lua_on_disk_done(event)
end
