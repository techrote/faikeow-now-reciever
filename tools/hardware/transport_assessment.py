"""Explicit FNR-005 physical-result assessments. Synthetic tests are not evidence."""
ESP_FIELDS = ('generated sent_data sent_status raw_written command_errors received accepted '
              'ingress_dropped ingress_consumed offered tx_replaced invalid_offers status_replaced '
              'aborted restarts frames_started frames_completed bytes_written hash_lo hash_hi '
              'next_id uart_overrun uart_rx_error generation_lo generation_hi').split()
PARSER_ERRORS = ('bad_cobs bad_crc bad_length bad_version bad_type bad_payload oversized_frames '
                 'sequence_discontinuities generation_changes link_discontinuities link_timeouts').split()


def esp_values(record):
    if record.get('event') != 'esp' or len(record.get('values', [])) != len(ESP_FIELDS):
        raise ValueError('Incomplete ESP telemetry')
    values = dict(zip(ESP_FIELDS, record['values']))
    if any(type(v) is not int or not 0 <= v <= 0xffffffff for v in values.values()):
        raise ValueError('Invalid ESP counters')
    return values


def delta(before, after, key):
    a, b = before[key], after[key]
    if type(a) is not int or type(b) is not int or not 0 <= a <= b < 0xffffffff:
        raise ValueError('Counter reset/saturation or invalid counter: ' + key)
    return b - a


def assess_traffic(rp_before, rp_after, esp_before, esp_after, *, count, length,
                   mode='clean', minimum_us=0):
    """Require drained snapshots. Replacement is allowed only at its named stage.

    ESP snapshots precede their own telemetry record. The previous telemetry is
    therefore exactly one additional offered/completed datagram in this delta;
    the new telemetry is already included in the RP after snapshot. Neither is
    included in the diagnostic sent_data/delivered/digest totals.
    """
    if mode not in ('clean', 'tx_overload', 'consumer_delay') or not 1 <= count <= 30000 or not 1 <= length <= 250:
        raise ValueError('Invalid assessment request')
    errors = []
    a, b = esp_values(esp_before), esp_values(esp_after)
    tx_drop = delta(a, b, 'tx_replaced')
    rx_drop = delta(rp_before['parser'], rp_after['parser'], 'datagrams_replaced')
    sent = delta(a, b, 'sent_data')
    delivered = delta(rp_before, rp_after, 'delivered')
    if delta(a, b, 'generated') != count or delta(a, b, 'accepted') != count:
        errors.append('source generation/admission disagreement')
    if delta(a, b, 'received') != count or delta(a, b, 'ingress_consumed') != count or delta(a, b, 'ingress_dropped'):
        errors.append('ingress accounting disagreement')
    if delta(a, b, 'offered') != count + 1 or sent + tx_drop != count:
        errors.append('TX ownership/replacement disagreement')
    if delta(a, b, 'frames_started') != delta(a, b, 'frames_completed'):
        errors.append('TX did not drain')
    status = delta(a, b, 'sent_status')
    if delta(a, b, 'frames_completed') != sent + status + 1:
        errors.append('TX completion disagreement')
    if delta(rp_before['parser'], rp_after['parser'], 'datagrams_received') != sent + 1:
        errors.append('wire/parser datagram disagreement')
    if delta(rp_before['parser'], rp_after['parser'], 'status_received') != status:
        errors.append('wire/parser status disagreement')
    if delta(rp_before['parser'], rp_after['parser'], 'frames_valid') != sent + status + 1:
        errors.append('validated frame accounting disagreement')
    wire = delta(a, b, 'bytes_written')
    minimum_wire = 202 + status * 68 + sent * (length + 26)
    maximum_wire = minimum_wire + sent * ((length + 24) // 254)
    if not minimum_wire <= wire <= maximum_wire:
        errors.append('encoded wire-size budget disagreement')
    if (delta(rp_before['uart'], rp_after['uart'], 'received') != wire or
            delta(rp_before['uart'], rp_after['uart'], 'popped') != wire or
            delta(rp_before['parser'], rp_after['parser'], 'bytes_received') != wire):
        errors.append('UART/ring/parser byte disagreement')
    if delivered + rx_drop != sent:
        errors.append('consumer ownership/replacement disagreement')
    if delta(rp_before['parser'], rp_after['parser'], 'datagrams_dropped') != rx_drop:
        errors.append('unexplained consumer retirement')
    for key in ('pattern_errors', 'metadata_errors', 'duplicates', 'backwards', 'commands_bad'):
        if delta(rp_before, rp_after, key): errors.append('observer ' + key)
    for key in ('command_errors', 'invalid_offers', 'aborted', 'raw_written', 'uart_overrun', 'uart_rx_error'):
        if delta(a, b, key): errors.append('ESP ' + key)
    for key in PARSER_ERRORS:
        if delta(rp_before['parser'], rp_after['parser'], key): errors.append('parser ' + key)
    for key in ('dropped', 'overflows', 'faults'):
        if delta(rp_before['uart'], rp_after['uart'], key): errors.append('ring ' + key)
    for key in ('framing', 'parity', 'breaks', 'overruns'):
        if delta(rp_before, rp_after, key): errors.append('UART ' + key)
    if any(rp_after[k] for k in ('datagram_pending', 'status_pending', 'encoded_pending', 'ring_pending')):
        errors.append('RP did not drain')
    if mode == 'clean' and (tx_drop or rx_drop or delta(rp_before, rp_after, 'gaps')):
        errors.append('unexpected clean loss/gap')
    if mode == 'tx_overload' and (not tx_drop or rx_drop):
        errors.append('overload not isolated to TX replacement')
    if mode == 'consumer_delay' and (not rx_drop or tx_drop):
        errors.append('delay not isolated to consumer replacement')
    if mode != 'consumer_delay':
        sender_hash = (b['hash_hi'] << 32) | b['hash_lo']
        if int(rp_after['digest'], 16) != sender_hash: errors.append('submitted/consumer payload digest disagreement')
    if delta(rp_before, rp_after, 'payload_bytes') != delivered * length:
        errors.append('consumer payload byte disagreement')
    if esp_after.get('active_us', -1) < minimum_us:
        errors.append('insufficient active duration')
    return errors
