#!/usr/bin/env perl
use strict;
use warnings;
use POSIX qw(strftime);
use FindBin qw($RealBin);

# Resolve folders relative to this CGI script so it works from any launch directory.
my $script_dir = $RealBin;
my $root_dir = "$script_dir/..";
my $archive_dir = "$root_dir/archive";
my $log_dir = "$root_dir/log";
my $log_file = "$log_dir/upload_script.log";

sub log_message {
    my ($msg) = @_;
    # Best-effort logging: failures here should not break request handling.
    eval {
        mkdir $log_dir unless -d $log_dir;
        open my $fh, '>>', $log_file or return;
        my $ts = strftime('%Y-%m-%d %H:%M:%S', localtime);
        print $fh "[$ts] [delete.pl] $msg\n";
        close $fh;
    };
}

sub json_escape {
    my ($s) = @_;
    # Escape characters that would break JSON string syntax.
    $s =~ s/\\/\\\\/g;
    $s =~ s/"/\\"/g;
    $s =~ s/\n/\\n/g;
    $s =~ s/\r/\\r/g;
    return $s;
}

sub send_response {
    my ($status, $ctype, $body) = @_;
    # Send CGI response headers and body with explicit content length.
    my $len = length($body);
    print "Status: $status\r\n";
    print "Content-Type: $ctype\r\n";
    print "Content-Length: $len\r\n\r\n";
    print $body;
}

sub list_files {
    # List regular files in the archive directory (case-insensitive sorted).
    return () unless -d $archive_dir;
    opendir(my $dh, $archive_dir) or return ();
    my @files = grep { -f "$archive_dir/$_" } readdir($dh);
    closedir($dh);
    @files = sort { lc($a) cmp lc($b) } @files;
    return @files;
}

sub invalid_name {
    my ($name) = @_;
    # Reject empty names and path traversal/path separator attempts.
    return 1 if !defined($name) || $name eq '';
    return 1 if $name =~ /\.\./;
    return 1 if $name =~ m{[\\/]};
    return 0;
}

sub parse_delete_name {
    my ($ctype, $body) = @_;
    # Support JSON and URL-encoded payloads; fallback to raw trimmed body.
    if ($ctype =~ /application\/json/i) {
        return $1 if $body =~ /"name"\s*:\s*"([^"]*)"/;
    }
    if ($ctype =~ /application\/x-www-form-urlencoded/i) {
        return $1 if $body =~ /(?:^|&)name=([^&]*)/;
    }
    $body =~ s/^\s+|\s+$//g;
    return $body;
}

sub uri_decode {
    my ($s) = @_;
    # Decode URL-encoded text such as %20 and + into plain text.
    $s =~ tr/+/ /;
    $s =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/eg;
    return $s;
}

sub html_page {
    # Return archive management UI that calls this same endpoint for list/delete.
    return <<'HTML';
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8" />
    <link rel="stylesheet" href="/style.css">
    <title>Archive</title>
</head>
<body class="body-top">
    <div id="header-placeholder"></div>
    <script src="/header.js"></script>
    <div class="hero-wrap">
        <div class="container form-card">
            <h2 class="form-title">Archive (Perl CGI)</h2>
            <p class="muted">Uploaded files stored on the server. Fetches live data from the Perl archive endpoint.</p>
            <div class="table-wrapper">
                <table class="archive-table" id="archive-table">
                    <thead>
                        <tr><th>Name</th><th style="width: 140px;">Actions</th></tr>
                    </thead>
                    <tbody id="archive-body">
                        <tr><td colspan="2" class="muted">Loading...</td></tr>
                    </tbody>
                </table>
            </div>
            <div class="actions" style="margin-top:1rem; display:flex; gap:0.5rem;">
                <button class="btn-primary" id="refresh-btn" type="button">Refresh</button>
                <span id="status" class="muted"></span>
            </div>
        </div>
    </div>

    <script>
    // fetchList - get data from server
    // renderRows - draw data on page(display)
    // Use the current CGI path as both the list endpoint and the delete endpoint.
    const API_LIST = window.location.pathname + '?format=json';
    const API_DELETE = window.location.pathname;

    async function fetchList() {
        // Grab the status text and table body so we can update the UI.
        const statusEl = document.getElementById('status');
        const bodyEl = document.getElementById('archive-body');
        statusEl.textContent = '';
        // Show a temporary loading row while waiting for the server reply.
        bodyEl.innerHTML = '<tr><td colspan="2" class="muted">Loading...</td></tr>';
        try {
            // Ask the CGI endpoint for the archive file list in JSON form.
            const res = await fetch(API_LIST, { headers: { 'Accept': 'application/json' } });
            if (!res.ok) throw new Error('Failed to load list');
            // Parse the JSON response and render the filenames into the table.
            const data = await res.json();
            renderRows(data.files || []);
        } catch (err) {
            // Replace the table with an error message if the request fails.
            bodyEl.innerHTML = '<tr><td colspan="2" class="muted">Error loading files.</td></tr>';
            statusEl.textContent = err.message;
        }
    }

    function renderRows(files) {
        // Reuse the same table body and status area for the rendered list.
        const bodyEl = document.getElementById('archive-body');
        const statusEl = document.getElementById('status');
        if (!files.length) {
            // Show an empty-state row when the archive has no files.
            bodyEl.innerHTML = '<tr><td colspan="2" class="muted">No files found.</td></tr>';
            return;
        }
        bodyEl.innerHTML = '';
        // Create one table row per file with a matching Delete button.
        files.forEach(name => {
            const tr = document.createElement('tr');
            const nameTd = document.createElement('td');
            nameTd.textContent = name;
            const actionTd = document.createElement('td');
            const btn = document.createElement('button');
            btn.textContent = 'Delete';
            btn.className = 'btn-primary';
            btn.onclick = async () => {
                // Tell the user which file is being deleted.
                statusEl.textContent = 'Deleting ' + name + '...';
                // Send the filename to the CGI endpoint as JSON.
                const resp = await fetch(API_DELETE, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name })
                });
                if (resp.ok) {
                    // On success, refresh the list so the deleted file disappears.
                    statusEl.textContent = 'Deleted ' + name;
                    fetchList();
                } else {
                    // On failure, keep the row and show the HTTP status.
                    statusEl.textContent = 'Delete failed (' + resp.status + ')';
                }
            };
            actionTd.appendChild(btn);
            tr.appendChild(nameTd);
            tr.appendChild(actionTd);
            bodyEl.appendChild(tr);
        });
    }

    // Wire the Refresh button to reload the archive list.
    document.getElementById('refresh-btn').addEventListener('click', fetchList);
    // Load the list once when the page opens.
    fetchList();
    </script>
</body>
</html>
HTML
}

sub main {
    # Route by method/query: JSON list, delete action, or HTML page.
    my $method = $ENV{'REQUEST_METHOD'} // 'GET';
    my $query = $ENV{'QUERY_STRING'} // '';
    log_message("Request started method=$method query=$query");

    # GET ?format=json -> return archive listing.
    if ($method eq 'GET' && $query =~ /(?:^|&)format=json(?:&|$)/) {
        my @files = list_files();
        my $json = '{"files":[' . join(',', map { '"' . json_escape($_) . '"' } @files) . ']}';
        log_message('Returned list count=' . scalar(@files));
        send_response('200 OK', 'application/json', $json);
        return;
    }

    # POST/DELETE -> read body, validate name, and delete the target file.
    if ($method eq 'POST' || $method eq 'DELETE') {
        my $len = $ENV{'CONTENT_LENGTH'} // 0;
        $len = 0 if $len !~ /^\d+$/;
        my $body = '';
        my $remaining = $len;
        while ($remaining > 0) {
            my $chunk = '';
            my $read = read(STDIN, $chunk, $remaining);
            if (!defined $read) {
                log_message('Failed reading request body');
                send_response('400 Bad Request', 'application/json', '{"success":false,"message":"Invalid request body"}');
                return;
            }
            last if $read == 0;
            $body .= $chunk;
            $remaining -= $read;
        }

        # Ensure full body was read before parsing.
        if (length($body) != $len) {
            log_message('Incomplete request body');
            send_response('400 Bad Request', 'application/json', '{"success":false,"message":"Invalid request body"}');
            return;
        }

        my $ctype = $ENV{'CONTENT_TYPE'} // '';
        my $name = parse_delete_name($ctype, $body);
        $name = uri_decode($name // '');

        # Block invalid names early before touching the filesystem.
        if (invalid_name($name)) {
            log_message("Rejected invalid filename: $name");
            send_response('400 Bad Request', 'application/json', '{"success":false,"message":"Invalid filename"}');
            return;
        }

        my $path = "$archive_dir/$name";
        # Return 404 when the requested file does not exist.
        if (!-e $path) {
            log_message("Not found: $name");
            send_response('404 Not Found', 'application/json', '{"success":false,"message":"Not found"}');
            return;
        }

        # Delete the file and report the result.
        if (unlink $path) {
            log_message("Deleted file: $name");
            send_response('200 OK', 'application/json', '{"success":true,"message":"Deleted"}');
            return;
        }

        log_message("Failed deleting file: $name");
        send_response('500 Internal Server Error', 'application/json', '{"success":false,"message":"Delete failed"}');
        return;
    }

    # Default response is the HTML archive page.
    send_response('200 OK', 'text/html', html_page());
}

# Entry point for CGI execution.
main();
