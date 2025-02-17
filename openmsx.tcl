proc peek_ieee_float32 {addr {debuggable "memory"}} {
    binary scan [debug read_block $debuggable $addr 4] f result
    return $result
}