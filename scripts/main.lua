function lua_on_tcp_message(event)
    cpp_send_tcp(event.session_id, event.payload)
end

function lua_on_udp_signal(event)
    cpp_send_udp(event.session_id, event.payload)
end

function lua_on_rtp(event)
    cpp_log("info", "rtp bytes=" .. tostring(#event.payload)
        .. " ssrc_id=" .. tostring(event.session_id)
        .. " from=" .. tostring(event.remote_ip) .. ":" .. tostring(event.remote_port))
end

function lua_on_timer(event)
    -- 可以在这里做周期性任务，例如心跳或定时检查
end

function lua_on_disk_done(event)
end
