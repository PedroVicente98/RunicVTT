<#
.SYNOPSIS
    Returns the primary IPv4 address of the local machine.

.DESCRIPTION
    This script identifies the most suitable IPv4 address based on several criteria:
    - It's not a loopback address (127.0.0.1).
    - It's not an APIPA address (169.254.x.x).
    - It's associated with a network interface that has a default IPv4 gateway.
    - Its ValidLifetime and PreferredLifetime properties are not empty or "00:00:00",
      indicating an actively assigned and valid address.
    - Its AddressState is 'Preferred'.
    - Its PrefixOrigin is 'Dhcp' or 'Manual'.

    The script outputs only the IPv4 address string to standard output and then exits
    upon finding the first matching address. If no suitable address is found, it outputs nothing.

.OUTPUTS
    System.String
        The primary IPv4 address, e.g., "192.168.1.50".
#>

# Configurar a estrita conformidade para ajudar a identificar erros
Set-StrictMode -Version Latest

# Function to check if a lifetime string is valid (not empty and not "00:00:00")
function Test-LifetimeIsValid {
    param (
        [string]$Lifetime
    )
    if ([string]::IsNullOrWhiteSpace($Lifetime)) {
        return $false
    }
    if ($Lifetime -eq "00:00:00") {
        return $false
    }
    return $true
}

# Get all IPv4 addresses
$ipv4Addresses = Get-NetIPAddress -AddressFamily IPv4

# Se nenhum endereço IPv4 for encontrado, sair com sucesso (nada para processar)
if ($null -eq $ipv4Addresses -or $ipv4Addresses.Count -eq 0) {
    exit 0
}

foreach ($ip in $ipv4Addresses) {
    # 1. Filter out Loopback (127.0.0.1) and APIPA (169.254.x.x) addresses
    if ($ip.IPAddress -eq "127.0.0.1" -or $ip.IPAddress -like "169.254.*") {
        continue # Pular para o próximo IP
    }

    # 2. Get the network configuration for this interface to check for a default IPv4 gateway
    $netConfig = Get-NetIPConfiguration -InterfaceIndex $ip.InterfaceIndex -ErrorAction SilentlyContinue

    $hasGateway = $false
    if ($null -ne $netConfig -and $null -ne $netConfig.IPv4DefaultGateway) {
        # Acessar o IP do gateway e verificar se não é nulo/vazio/0.0.0.0
        $gatewayIp = $netConfig.IPv4DefaultGateway[0].ToString() 
        if (-not ([string]::IsNullOrWhiteSpace($gatewayIp)) -and $gatewayIp -ne "0.0.0.0") {
            $hasGateway = $true
        }
    }

    if (-not $hasGateway) {
        continue # Pular se não houver gateway válido para esta interface
    }

    # 3. Check ValidLifetime and PreferredLifetime (devem ser preenchidos e não "00:00:00")
    if (-not (Test-LifetimeIsValid $ip.ValidLifetime) -or -not (Test-LifetimeIsValid $ip.PreferredLifetime)) {
        continue # Pular se os tempos de vida não forem válidos
    }

    # 4. Check AddressState (deve ser 'Preferred')
    if ($ip.AddressState -ne "Preferred") {
        continue # Pular se o estado do endereço não for 'Preferred'
    }

    # 5. Check PrefixOrigin (deve ser 'Dhcp' ou 'Manual')
    if ($ip.PrefixOrigin -ne "Dhcp" -and $ip.PrefixOrigin -ne "Manual") {
        continue # Pular se a origem do prefixo não for 'Dhcp' ou 'Manual'
    }

    # Se todas as verificações passarem, este é o endereço IPv4 correto.
    # Imprimir apenas o endereço IP e sair com código de sucesso (0).
    Write-Output $ip.IPAddress
    exit 0
}

# Se o loop terminar sem encontrar um endereço IPv4 adequado, sair com código de erro (1).
exit 1