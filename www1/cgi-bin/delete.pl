#!/usr/bin/env perl
use strict;
use warnings;
use POSIX qw(strftime);

my $script_dir = $0;
$script_dir =~ s{/[^/]+$}{};
my $root_dir = "$script_dir/..";
my $archive_dir = "$root_dir/archive";
my $log_dir = "$root_dir/log";
my $log_file = "$log_dir/upload_script.log";

sub log_message {
    my ($msg) = @_;
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
    $s =~ s/\\/\\\\/g;
    $s =~ s/"/\\"/g;
    $s =~ s/\n/\\n/g;
    $s =~ s/\r/\\r/g;
    return $s;
}

sub send_response {
    my ($status, $ctype, $body) = @_;
    my $len = length($body);
    print "Status: $status\r\n";
    print "Content-Type: $ctype\r\n";
    print "Content-Length: $len\r\n\r\n";
    print $body;
}

sub list_files {
    return () unless -d $archive_dir;
    opendir(my $dh, $archive_dir) or return ();
    my @files = grep { -f "$archive_dir/$_" } readdir($dh);
    closedir($dh);
    @files = sort { lc($a) cmp lc($b) } @files;
    return @files;
}

sub invalid_name {
    my ($name) = @_;
    return 1 if !defined($name) || $name eq '';
    return 1 if $name =~ /\.\./;
    return 1 if $name =~ m{[\\/]};
    return 0;
}

sub parse_delete_name {
    my ($ctype, $body) = @_;
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
    $s =~ tr/+/ /;
    $s =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/eg;
    return $s;
}

sub html_page {
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
    const API_LIST = window.location.pathname + '?format=json';
    const API_DELETE = window.location.pathname;

    async function fetchList() {
        const statusEl = document.getElementById('status');
        const bodyEl = document.getElementById('archive-body');
        statusEl.textContent = '';
        bodyEl.innerHTML = '<tr><td colspan="2" class="muted">Loading...</td></tr>';
        try {
            const res = await fetch(API_LIST, { headers: { 'Accept': 'application/json' } });
            if (!res.ok) throw new Error('Failed to load list');
            const data = await res.json();
            renderRows(data.files || []);
        } catch (err) {
            bodyEl.innerHTML = '<tr><td colspan="2" class="muted">Error loading files.</td></tr>';
            statusEl.textContent = err.message;
        }
    }

    function renderRows(files) {
        const bodyEl = document.getElementById('archive-body');
        const statusEl = document.getElementById('status');
        if (!files.length) {
            bodyEl.innerHTML = '<tr><td colspan="2" class="muted">No files found.</td></tr>';
            return;
        }
        bodyEl.innerHTML = '';
        files.forEach(name => {
            const tr = document.createElement('tr');
            const nameTd = document.createElement('td');
            nameTd.textContent = name;
            const actionTd = document.createElement('td');
            const btn = document.createElement('button');
            btn.textContent = 'Delete';
            btn.className = 'btn-primary';
            btn.onclick = async () => {
                statusEl.textContent = 'Deleting ' + name + '...';
                const resp = await fetch(API_DELETE, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name })
                });
                if (resp.ok) {
                    statusEl.textContent = 'Deleted ' + name;
                    fetchList();
                } else {
                    statusEl.textContent = 'Delete failed (' + resp.status + ')';
                }
            };
            actionTd.appendChild(btn);
            tr.appendChild(nameTd);
            tr.appendChild(actionTd);
            bodyEl.appendChild(tr);
        });
    }

    document.getElementById('refresh-btn').addEventListener('click', fetchList);
    fetchList();
    </script>
</body>
</html>
HTML
}

sub main {
    my $method = $ENV{'REQUEST_METHOD'} // 'GET';
    my $query = $ENV{'QUERY_STRING'} // '';
    log_message("Request started method=$method query=$query");

    if ($method eq 'GET' && $query =~ /(?:^|&)format=json(?:&|$)/) {
        my @files = list_files();
        my $json = '{"files":[' . join(',', map { '"' . json_escape($_) . '"' } @files) . ']}';
        log_message('Returned list count=' . scalar(@files));
        send_response('200 OK', 'application/json', $json);
        return;
    }

    if ($method eq 'POST' || $method eq 'DELETE') {
        my $len = $ENV{'CONTENT_LENGTH'} // 0;
        $len = 0 if $len !~ /^\d+$/;
        my $body = '';
        read(STDIN, $body, $len) if $len > 0;
        my $ctype = $ENV{'CONTENT_TYPE'} // '';
        my $name = parse_delete_name($ctype, $body);
        $name = uri_decode($name // '');

        if (invalid_name($name)) {
            log_message("Rejected invalid filename: $name");
            send_response('400 Bad Request', 'application/json', '{"success":false,"message":"Invalid filename"}');
            return;
        }

        my $path = "$archive_dir/$name";
        if (!-e $path) {
            log_message("Not found: $name");
            send_response('404 Not Found', 'application/json', '{"success":false,"message":"Not found"}');
            return;
        }

        if (unlink $path) {
            log_message("Deleted file: $name");
            send_response('200 OK', 'application/json', '{"success":true,"message":"Deleted"}');
            return;
        }

        log_message("Failed deleting file: $name");
        send_response('500 Internal Server Error', 'application/json', '{"success":false,"message":"Delete failed"}');
        return;
    }

    send_response('200 OK', 'text/html', html_page());
}

main();
