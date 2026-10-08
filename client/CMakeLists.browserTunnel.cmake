# Pow VPN Browser Tunnel native messaging host.
# Include from the branded client CMakeLists after Qt6 is found.
add_executable(pow-native-host
    ${CMAKE_CURRENT_LIST_DIR}/core/browserTunnel/browserTunnelNativeHost.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/browserTunnel/browserTunnelNativeTransport.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/browserTunnel/powNativeHostMain.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/browserTunnel/browserTunnelService.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/browserTunnel/browserTunnelService.h
)
target_include_directories(pow-native-host PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(pow-native-host PRIVATE Qt6::Core Qt6::Network)
set_target_properties(pow-native-host PROPERTIES OUTPUT_NAME "pow-native-host")
install(FILES ${CMAKE_CURRENT_LIST_DIR}/browserTunnel/register-native-host.ps1
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT AmneziaVPN
)