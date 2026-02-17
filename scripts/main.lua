function lua_on_tcp_message(event)
    print("lua_on_tcp_message protocol=" .. tostring(event.protocol)
        .. " session_id=" .. tostring(event.session_id)
        .. " remote=" .. tostring(event.remote_ip) .. ":" .. tostring(event.remote_port)
        .. " size=" .. tostring(#event.payload))
end

function lua_on_udp_signal(event)
    print("lua_on_udp_signal protocol=" .. tostring(event.protocol)
        .. " session_id=" .. tostring(event.session_id)
        .. " remote=" .. tostring(event.remote_ip) .. ":" .. tostring(event.remote_port)
        .. " size=" .. tostring(#event.payload))
end

function lua_on_timer(event)
end

function lua_on_disk_done(event)
end

