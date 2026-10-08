# Pow VPN Client

[Pow VPN](https://powvpn.com) is a privacy-focused VPN service with applications for Windows, Linux and Android. The client provides account-based access to Pow VPN locations and supports modern VPN transports, including AmneziaWG 3.1, WireGuard, VLESS Reality and IKEv2/IPsec where available.

This repository contains the open-source Pow VPN client. It is a modified distribution of the [Amnezia VPN client](https://github.com/amnezia-vpn/amnezia-client), with Pow VPN branding, account integration, managed server selection and browser-extension integration.

## Pow VPN changes

- Pow VPN branding, application identifiers, icons and installer metadata.
- Authentication with a Pow VPN account and managed device registration.
- Automatic and country-based server selection from the Pow VPN network.
- Managed connection flow with protocol fallback.
- Native messaging bridge for the Pow VPN browser extensions.
- Pow VPN-specific user interface, translations and release packaging.

## Upstream project

Pow VPN Client is derived from `amnezia-vpn/amnezia-client` and retains its Git history, copyright notices and GNU GPL v3 license. The initial Pow VPN modification set in this repository is based on upstream commit [`94b51df24790bf52427afe82d81c87a95460bdfd`](https://github.com/amnezia-vpn/amnezia-client/commit/94b51df24790bf52427afe82d81c87a95460bdfd).

Pow VPN is an independent project and is not affiliated with or endorsed by the Amnezia VPN project. See [NOTICE.md](NOTICE.md) for attribution details and [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for third-party components.

## Building

Clone the repository with its submodules:

```bash
git clone --recurse-submodules https://github.com/powvpn/powvpn-client.git
cd powvpn-client
```

The project uses CMake, Qt 6 and Conan. Platform-specific prerequisites and build behavior follow the upstream Amnezia client architecture. Build outputs, signing keys, keystores, local configuration and generated dependency files are intentionally excluded from this repository.

Windows builds use the project build script from a configured Qt/MSVC environment:

```powershell
.\deploy\build.bat
```

Never commit signing credentials, private keys, account tokens or production secrets. Release signing and store credentials must be supplied outside the source tree.

## Service and downloads

- Website: [https://powvpn.com](https://powvpn.com)
- Downloads: [https://powvpn.com/download](https://powvpn.com/download)
- Privacy policy: [https://powvpn.com/privacy-policy](https://powvpn.com/privacy-policy)

## Security

Please do not publish exploitable security issues or credentials in a public issue. Report security concerns privately to `info@powvpn.com`.

## License

This project is distributed under the [GNU General Public License v3.0](LICENSE), consistent with the upstream Amnezia VPN client. Modified source code is provided under the same license. Third-party components remain subject to their respective licenses listed in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

The Pow VPN name and logos identify the Pow VPN service. The GPL license applies to the software source code and does not grant trademark rights.
