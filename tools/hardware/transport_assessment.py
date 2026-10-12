"""Explicit FNR-005 physical-result assessments. Synthetic tests are not evidence."""
ESP_FIELDS = ('generated sent_data sent_status raw_written command_errors received accepted '
              'ingress_dropped ingress_consumed offered tx_replaced invalid_offers status_replaced '
              'aborted restarts frames_started frames_completed bytes_written hash_lo hash_hi '
              'next_id uart_overrun uart_rx_error generation_lo generation_hi').split()
PARSER_ERRORS = ('bad_cobs bad_crc bad_length bad_version bad_type bad_payload oversized_frames '
                 'sequence_discontinuities generation_changes link_discontinuities link_timeouts '
                 'bytes_discarded parser_resets radio_resets radio_unavailable').split()


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


def qualification_digest(first, count, length, initial=14695981039346656037):
    if not (0 <= first <= 0xffffffff - count and 0 <= count <= 30000 and 1 <= length <= 250):
        raise ValueError('Opaque pattern digest bounds')
    digest = initial
    for marker in range(first, first + count):
        for offset in range(length):
            value = ((marker >> (8 * offset)) & 255) if offset < 4 else (((marker * 37 + offset * 73) ^ (offset >> 1)) & 255)
            digest = ((digest ^ value) * 1099511628211) & 0xffffffffffffffff
    return digest


def assess_traffic(rp_before, rp_after, esp_before, esp_after, *, count, length,
                   mode='clean', minimum_us=0, first=0):
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
    for snapshot, values in ((rp_before, a), (rp_after, b)):
        if int(snapshot['generation'], 16) != (values['generation_hi'] << 32) | values['generation_lo']:
            errors.append('ESP/receiver generation disagreement')
        if any(snapshot[k] for k in ('datagram_pending', 'status_pending', 'encoded_pending', 'ring_pending')):
            errors.append('measurement boundary did not drain')
    if esp_before.get('source') != esp_after.get('source'):
        errors.append('source changed inside phase')
    validated = delta(rp_before['parser'], rp_after['parser'], 'frames_valid')
    if (rp_after['sequence'] - rp_before['sequence']) & 0xffffffff != validated:
        errors.append('modulo transport sequence disagreement')
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
    if delta(rp_before['parser'], rp_after['parser'], 'datagrams_consumed') != delivered + 1:
        errors.append('consumer handoff counter disagreement')
    if delta(rp_before['parser'], rp_after['parser'], 'datagrams_dropped') != rx_drop:
        errors.append('unexplained consumer retirement')
    status_drop = delta(rp_before['parser'], rp_after['parser'], 'status_dropped')
    if (delta(rp_before['parser'], rp_after['parser'], 'status_consumed') != delta(rp_before, rp_after, 'statuses') or
            delta(rp_before, rp_after, 'statuses') + status_drop != status or
            status_drop != delta(rp_before['parser'], rp_after['parser'], 'status_replaced') or
            (mode != 'consumer_delay' and status_drop)):
        errors.append('status handoff/replacement disagreement')
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
    else:
        expected = qualification_digest(first + count - delivered, delivered, length, int(rp_before['digest'], 16))
        if int(rp_after['digest'], 16) != expected:
            errors.append('consumer delivery is not exactly the newest suffix')
    if delta(rp_before, rp_after, 'payload_bytes') != delivered * length:
        errors.append('consumer payload byte disagreement')
    if esp_after.get('active_us', -1) < minimum_us:
        errors.append('insufficient active duration')
    return errors


def assess_corruption(before, after, *, expected_rejections, oversized=False, empty=False,
                      esp_before=None, esp_after=None, fault=None):
    """Assess a bad-only phase between drained telemetry boundaries.

    Exactly one new telemetry datagram is valid; no pattern record may escape.
    Recovery traffic is separately assessed with assess_traffic.
    """
    errors = []
    reject_names = ['bad_cobs', 'bad_crc', 'bad_length', 'bad_version', 'bad_type', 'bad_payload', 'oversized_frames']
    observed = {key: delta(before['parser'], after['parser'], key) for key in reject_names}
    if sum(observed.values()) != expected_rejections:
        errors.append('rejection count disagreement')
    if oversized and observed['oversized_frames'] != expected_rejections:
        errors.append('oversize boundary not observed')
    if empty and delta(before['parser'], after['parser'], 'empty_delimiters') < 8:
        errors.append('repeated delimiters not observed')
    if delta(before, after, 'delivered') or delta(before, after, 'statuses'):
        errors.append('invalid payload escaped')
    if delta(before['parser'], after['parser'], 'frames_valid') != 1:
        errors.append('unexpected valid frame or absent telemetry')
    for key in ('pattern_errors', 'metadata_errors', 'duplicates', 'backwards', 'commands_bad'):
        if delta(before, after, key): errors.append('observer ' + key)
    for key in ('framing', 'parity', 'breaks', 'overruns'):
        if delta(before, after, key): errors.append('unrelated UART fault')
    if delta(before['uart'], after['uart'], 'overflows'):
        errors.append('unrelated ring overflow')
    if any(s[k] for s in (before, after) for k in ('datagram_pending', 'status_pending', 'encoded_pending', 'ring_pending')):
        errors.append('corruption boundary did not drain')
    if esp_before is not None:
        a, b = esp_values(esp_before), esp_values(esp_after)
        if fault not in range(13): raise ValueError('Unknown terminated corruption stimulus')
        raw_sizes = (8, 30, 59, 57, 58, 58, 58, 58, 58, 58, 277, 278, 94)
        raw = delta(a, b, 'raw_written')
        wire = delta(a, b, 'bytes_written') + raw
        if raw != raw_sizes[fault] or delta(a, b, 'bytes_written') != 202:
            errors.append('corruption stimulus byte disagreement')
        if any(delta(a, b, k) for k in ('generated', 'received', 'accepted', 'ingress_consumed',
               'ingress_dropped', 'tx_replaced', 'aborted', 'command_errors', 'sent_data', 'sent_status')):
            errors.append('unexpected source traffic/loss')
        if any(delta(before['uart'], after['uart'], k) for k in ('dropped', 'faults')):
            errors.append('unexpected ring loss')
        if not (delta(before['uart'], after['uart'], 'received') ==
                delta(before['uart'], after['uart'], 'popped') ==
                delta(before['parser'], after['parser'], 'bytes_received') == wire):
            errors.append('corruption wire/parser byte disagreement')
        if not (delta(before['parser'], after['parser'], 'datagrams_received') ==
                delta(before['parser'], after['parser'], 'datagrams_consumed') == 1):
            errors.append('corruption recovery telemetry missing')
        if before['generation'] != after['generation'] or esp_before['source'] != esp_after['source']:
            errors.append('unexpected source/generation boundary')
    return errors


def assess_interruption(case, before, after, esp_before, esp_after, observations):
    """Predeclared reset/timeout/ring/UART cases; never infer RX loss from TX aborts."""
    if case not in ('partial-parser-reset', 'watchdog-delimiters', 'foreground-stall-small',
                    'foreground-stall-repeat', 'consumer-timeout', 'ring-overflow', 'uart-overrun'):
        raise ValueError('Unknown interruption case')
    errors = []
    def need(condition, reason):
        if not condition: errors.append(reason)
    p = {k: delta(before['parser'], after['parser'], k) for k in before['parser']}
    u = {k: delta(before['uart'], after['uart'], k) for k in before['uart']}
    a, b = esp_values(esp_before), esp_values(esp_after)
    for k in ('pattern_errors', 'metadata_errors', 'duplicates', 'backwards', 'commands_bad'):
        need(delta(before, after, k) == 0, 'observer ' + k)
    need(all(after[k] == 0 for k in ('datagram_pending', 'status_pending', 'encoded_pending', 'ring_pending')), 'final ownership not drained')
    need(before['generation'] == after['generation'], 'unexpected generation')
    need(int(after['generation'], 16) == (b['generation_hi'] << 32) | b['generation_lo'], 'ESP/RP generation disagreement')
    need(esp_before['source'] == esp_after['source'], 'source changed')
    need(delta(a, b, 'command_errors') == 0, 'ESP rejected stimulus')
    need(u['received'] == u['popped'] + u['dropped'] and u['popped'] == p['bytes_received'], 'ring/parser byte disagreement')
    wire = delta(a, b, 'bytes_written') + delta(a, b, 'raw_written')
    if case != 'uart-overrun': need(u['received'] == wire, 'unexplained wire loss')
    need(p['datagrams_received'] == p['datagrams_consumed'] + p['datagrams_dropped'], 'handoff retirement disagreement')
    if case in ('ring-overflow', 'uart-overrun'):
        for k in ('generated', 'received', 'accepted', 'ingress_consumed', 'sent_data'):
            need(delta(a, b, k) == 60, 'source stage ' + k)
        need(delta(a, b, 'tx_replaced') == delta(a, b, 'aborted') == delta(a, b, 'ingress_dropped') == 0, 'unexpected sender loss')
        need(p['link_discontinuities'] >= 1 and after['events'] & 2 and delta(before, after, 'event_batches') >= 1, 'missing loss boundary/event')
        for k in ('bad_cobs', 'bad_crc', 'bad_length', 'bad_version', 'bad_type', 'bad_payload', 'oversized_frames', 'generation_changes', 'link_timeouts'):
            need(p[k] == 0, 'unexpected parser ' + k)
        if case == 'ring-overflow':
            need(u['overflows'] == 1 and u['dropped'] > 1024 and delta(before, after, 'overruns') == 0, 'ring stimulus not established')
        else:
            need(delta(before, after, 'overruns') >= 1 and u['faults'] >= 1 and u['overflows'] == 0 and u['received'] < wire, 'UART stimulus not established')
        delivered = delta(before, after, 'delivered')
        need(0 < delivered < 60 and p['datagrams_replaced'] == 0, 'missing recovery/ownership')
        first = 200000 if case == 'ring-overflow' else 300000
        need(int(after['digest'], 16) == qualification_digest(first + 60 - delivered, delivered, 250, int(before['digest'], 16)), 'non-suffix/stale recovery delivery')
    else:
        need(delta(before, after, 'delivered') == 0, 'retired normal data escaped')
        need(before['digest'] == after['digest'] and delta(before, after, 'payload_bytes') == 0, 'retired payload changed observer digest')
        need(p['datagrams_consumed'] == 1, 'only recovery telemetry may be consumed')
        for k in ('bad_cobs', 'bad_crc', 'bad_length', 'bad_version', 'bad_type', 'bad_payload', 'oversized_frames', 'generation_changes', 'sequence_discontinuities', 'link_discontinuities'):
            need(p[k] == 0, 'unexpected parser ' + k)
        for k in ('framing', 'parity', 'breaks', 'overruns'):
            need(delta(before, after, k) == 0, 'unexpected UART ' + k)
        need(u['overflows'] == 0, 'unexpected ring overflow')
        if case == 'partial-parser-reset':
            partial = next((r for r in observations if r.get('event') == 'snapshot'), None)
            need(partial is not None and partial['encoded_pending'] == 29, 'partial input not held')
            need(p['parser_resets'] == 1 and p['link_timeouts'] == 0 and p['frames_valid'] == 1 and u['dropped'] == 0, 'parser reset retirement')
        else:
            wanted = 2 if case == 'foreground-stall-repeat' else 1
            need(p['link_timeouts'] == wanted and u['faults'] == wanted, 'watchdog boundary count')
            need(after['events'] & 256 and delta(before, after, 'event_batches') >= 1, 'timeout event not exposed')
            if case.startswith('foreground-stall'):
                need(u['dropped'] == 478 * wanted and p['frames_valid'] == 1, 'buffered input refreshed retired liveness')
            elif case == 'consumer-timeout':
                need(p['frames_valid'] == 3 and p['datagrams_replaced'] == 1 and p['datagrams_dropped'] == 2 and u['dropped'] == 0, 'consumer timeout retirement')
            else:
                need(p['frames_valid'] == 1 and p['empty_delimiters'] >= 8 and u['dropped'] == 0, 'delimiter watchdog behavior')
    return errors


def assess_lifecycle(before, after, esp_before, esp_after, pause, during):
    """Active 180-record phase with one observed partial sender-frame abort."""
    errors = []
    def need(condition, reason):
        if not condition: errors.append(reason)
    a, b = esp_values(esp_before), esp_values(esp_after)
    tx = {k: delta(a, b, k) for k in ('generated', 'received', 'accepted', 'ingress_consumed',
          'ingress_dropped', 'offered', 'tx_replaced', 'aborted', 'restarts', 'frames_started',
          'frames_completed', 'bytes_written', 'sent_data', 'sent_status')}
    p = {k: delta(before['parser'], after['parser'], k) for k in before['parser']}
    u = {k: delta(before['uart'], after['uart'], k) for k in before['uart']}
    need(all(tx[k] == 180 for k in ('generated', 'received', 'accepted', 'ingress_consumed')), 'source-stage disagreement')
    need(tx['ingress_dropped'] == tx['tx_replaced'] == 0 and tx['offered'] == 181, 'source ownership disagreement')
    need(esp_before['source'] == esp_after['source'], 'source changed')
    for key in ('command_errors', 'invalid_offers', 'uart_overrun', 'uart_rx_error'):
        need(delta(a, b, key) == 0, 'ESP ' + key)
    need(tx['sent_data'] == 179 and tx['aborted'] == tx['restarts'] == 1, 'active abort/restart not established')
    need(tx['frames_started'] - tx['frames_completed'] == 1, 'aborted active-frame accounting')
    need(p['datagrams_received'] == tx['sent_data'] + 1 and p['status_received'] == tx['sent_status'] and p['frames_valid'] == tx['frames_completed'], 'wire/validated-frame disagreement')
    need(u['received'] == u['popped'] == p['bytes_received'] == tx['bytes_written'], 'wire-byte disagreement')
    need(p['generation_changes'] == p['radio_resets'] == 1 and p['sequence_discontinuities'] == 0, 'generation/reset boundary')
    generation = (int(before['generation'], 16) + 1) & 0xffffffffffffffff
    need(int(after['generation'], 16) == generation == (b['generation_hi'] << 32) | b['generation_lo'], 'new generation mismatch')
    need(during['generation'] == after['generation'] and during['delivered'] == pause['delivered'], 'old delivery across delayed boundary')
    need(after['events'] & 4 and after['events'] & 16 and after['event_batches'] > pause['event_batches'], 'sticky generation/reset event lost')
    need(p['bad_cobs'] == 1, 'aborted partial frame was not rejected')
    for k in ('bad_crc', 'bad_length', 'bad_version', 'bad_type', 'bad_payload', 'oversized_frames', 'link_discontinuities', 'link_timeouts'):
        need(p[k] == 0, 'unexpected parser ' + k)
    need(all(u[k] == 0 for k in ('dropped', 'overflows', 'faults')), 'unexpected ring loss')
    need(p['datagrams_received'] == p['datagrams_consumed'] + p['datagrams_dropped'], 'consumer accounting')
    need(p['datagrams_dropped'] == p['datagrams_replaced'] + 1, 'old pending slot not explicitly retired')
    need(p['status_received'] == p['status_consumed'] + p['status_dropped'], 'status accounting')
    prefix = delta(before, pause, 'delivered')
    suffix = delta(pause, after, 'delivered')
    need(prefix + suffix <= 179 and suffix > 0, 'missing post-boundary delivery')
    digest = qualification_digest(400000, prefix, 250, int(before['digest'], 16))
    digest = qualification_digest(400180 - suffix, suffix, 250, digest)
    need(digest == int(after['digest'], 16), 'old/intermediate record replayed instead of fresh suffix')
    for k in ('pattern_errors', 'metadata_errors', 'duplicates', 'backwards', 'commands_bad', 'framing', 'parity', 'breaks', 'overruns'):
        need(delta(before, after, k) == 0, 'observer/UART ' + k)
    need(all(after[k] == 0 for k in ('datagram_pending', 'status_pending', 'encoded_pending', 'ring_pending')), 'final ownership not drained')
    return errors


def assess_published_phase(row):
    """Replay the curated observations, not their stored PASS strings."""
    observed = row['observations']
    case = row['test_kind']
    before, after = observed['rp_before'], observed['rp_after']
    if case.startswith('corruption-'):
        fault = int(case.rsplit('-', 1)[1])
        return assess_corruption(before, after, expected_rejections=int(fault != 0),
                                 oversized=fault == 11, empty=fault == 0,
                                 esp_before=observed['esp_before'], esp_after=observed['esp_after'], fault=fault)
    eb, ea = observed['esp_before'], observed['esp_after']
    if 'mode' in observed:
        run = next(s['command'].split() for s in row['stimulus_plan']
                   if s.get('command', '').startswith('X RUN '))
        return assess_traffic(before, after, eb, ea, count=observed['count'],
                              length=observed['length'], mode=observed['mode'],
                              first=int(run[-1]), minimum_us=600000000 if case == 'sustained-600' else 0)
    if case == 'esp-lifecycle':
        return assess_lifecycle(before, after, eb, ea, observed['pause'], observed['during_boundary'])
    return assess_interruption(case, before, after, eb, ea, row['phase_snapshots'])


def assess_rp_restart(initial, after, esp_before, esp_after, prior_pending, *, first, count, length):
    """Independent RP reload: source remains active; RP counters start a new epoch.

    Traffic during the observer's absence is intentionally unobserved. Source
    counters reconcile the whole finite run; receiver counters describe only
    the new observer epoch and must deliver exactly a fresh contiguous suffix.
    """
    errors = []
    def need(condition, reason):
        if not condition: errors.append(reason)
    a, b = esp_values(esp_before), esp_values(esp_after)
    for key in ('generated', 'received', 'accepted', 'ingress_consumed', 'sent_data'):
        need(delta(a, b, key) == count, 'source stage ' + key)
    for key in ('tx_replaced', 'ingress_dropped', 'aborted', 'command_errors', 'invalid_offers', 'restarts'):
        need(delta(a, b, key) == 0, 'unexpected source loss/error ' + key)
    need(delta(a, b, 'offered') == count + 1, 'source offers')
    status = delta(a, b, 'sent_status')
    need(delta(a, b, 'frames_completed') == delta(a, b, 'frames_started') == count + status + 1, 'source frame accounting')
    wire = delta(a, b, 'bytes_written')
    need(202 + status * 68 + count * (length + 26) <= wire <=
         202 + status * 68 + count * (length + 26 + (length + 24) // 254), 'source wire budget')
    need(esp_before['source'] == esp_after['source'], 'firmware source changed')
    need((a['generation_lo'], a['generation_hi']) == (b['generation_lo'], b['generation_hi']), 'ESP restarted during RP reload')
    need(prior_pending['datagram_pending'] == 1, 'old consumer slot was not held before RP reset')
    n = after['delivered']
    need(0 < n < count, 'independent outage/recovery not established')
    need(int(after['digest'], 16) == qualification_digest(first + count - n, n, length), 'non-suffix/stale delivery')
    need(after['payload_bytes'] == n * length, 'consumer byte accounting')
    for key in ('pattern_errors', 'metadata_errors', 'duplicates', 'backwards', 'commands_bad'):
        need(after[key] == 0, 'new observer ' + key)
    for key in ('bad_cobs', 'bad_crc', 'bad_length', 'bad_version', 'bad_type', 'bad_payload',
                'oversized_frames', 'sequence_discontinuities', 'generation_changes',
                'link_discontinuities', 'link_timeouts', 'datagrams_replaced', 'datagrams_dropped'):
        need(after['parser'][key] == initial['parser'][key], 'new post-recovery parser fault ' + key)
    for key in ('dropped', 'overflows', 'faults'):
        need(after['uart'][key] == initial['uart'][key], 'new post-recovery ring fault ' + key)
    for key in ('framing', 'parity', 'breaks', 'overruns'):
        need(after[key] == initial[key], 'new post-recovery UART fault ' + key)
    need(not initial['uart']['faults'] or initial['events'] & (2 | 256), 'initial fault lacks event visibility')
    need(after['events'] & 1 and after['parser']['frames_valid'] == n + after['statuses'] + 1, 'new epoch reset/frame accounting')
    need(after['parser']['datagrams_received'] == after['parser']['datagrams_consumed'] == n + 1, 'new epoch handoff accounting')
    need(after['uart']['received'] == after['uart']['popped'] + after['uart']['dropped'] and
         after['uart']['popped'] == after['parser']['bytes_received'], 'new epoch byte accounting')
    need(int(after['generation'], 16) == (b['generation_hi'] << 32) | b['generation_lo'], 'generation mismatch')
    need(all(after[k] == 0 for k in ('datagram_pending', 'status_pending', 'encoded_pending', 'ring_pending')), 'final ownership not drained')
    return errors
