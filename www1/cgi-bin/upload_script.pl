#!/usr/bin/env perl
use strict;
use warnings;
use POSIX qw(strftime);

my $script_dir = $0;
$script_dir =~ s{/[^/]+$}{};
my $root_dir = "$script_dir/..";
my $upload_dir = "$root_dir/archive";
my $log_dir = "$root_dir/log";
my $log_file = "$log_dir/upload_script.log";

sub log_message {
    my ($msg) = @_;
    eval {
        mkdir $log_dir unless -d $log_dir;
        open my $fh, '>>', $log_file or return;
        my $ts = strftime('%Y-%m-%d %H:%M:%S', localtime);
        print $fh "[$ts] [upload_script.pl] $msg\n";
        close $fh;
    };
}

sub redirect_response {
    my ($location) = @_;
    print "Status: 303 See Other\r\n";
    print "Location: $location\r\n";
    print "Content-Type: text/html\r\n";
    print "Content-Length: 0\r\n\r\n";
}

sub sanitize_filename {
    my ($name) = @_;
    $name =~ s/^.*[\\\/]//;
    $name =~ s/[\r\n]//g;
    $name =~ s/[\\\/]/_/g;
    return $name;
}

sub parse_multipart {
    my ($ctype, $body) = @_;
    return (undef, undef) unless $ctype =~ /boundary=([^;]+)/;
    my $boundary = $1;
    $boundary =~ s/^"|"$//g;
    my $marker = '--' . $boundary;

    my @parts = split(/\Q$marker\E/, $body);
    foreach my $part (@parts) {
        next if $part =~ /^--\s*$/;
        $part =~ s/^\r?\n//;
        $part =~ s/\r?\n$//;
        next unless $part =~ /\r\n\r\n/s;
        my ($headers, $content) = split(/\r\n\r\n/, $part, 2);

        next unless $headers =~ /Content-Disposition:\s*form-data;[^\r\n]*name="file1"/i;
        next unless $headers =~ /filename="([^"]*)"/i;
        my $filename = $1;
        $content =~ s/\r\n$//;
        $content =~ s/--$//;
        return ($filename, $content);
    }

    return (undef, undef);
}

sub random_hex8 {
    my @h = ('0'..'9', 'a'..'f');
    my $s = '';
    for (1..8) { $s .= $h[int(rand(@h))]; }
    return $s;
}

sub main {
    log_message('Request started');

    my $method = $ENV{'REQUEST_METHOD'} // '';
    if ($method ne 'POST') {
        log_message("Invalid method: $method");
        print "Status: 405 Method Not Allowed\r\nContent-Type: text/plain\r\n\r\nOnly POST requests are allowed";
        return;
    }

    my $ctype = $ENV{'CONTENT_TYPE'} // '';
    if ($ctype !~ m{^multipart/form-data}i) {
        log_message("Invalid content type: $ctype");
        print "Status: 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nExpected multipart/form-data";
        return;
    }

    my $len = $ENV{'CONTENT_LENGTH'} // 0;
    $len = 0 if $len !~ /^\d+$/;
    if ($len <= 0) {
        log_message('Empty request body');
        redirect_response('/failure_upload.html');
        return;
    }

    my $body = '';
    read(STDIN, $body, $len);

    my ($orig, $file_bytes) = parse_multipart($ctype, $body);
    if (!defined $orig || !defined $file_bytes || $orig eq '') {
        log_message('No file1 field in multipart payload');
        redirect_response('/failure_upload.html');
        return;
    }

    my $safe_orig = sanitize_filename($orig);
    if ($safe_orig eq '') {
        log_message('Filename sanitization failed');
        redirect_response('/failure_upload.html');
        return;
    }

    mkdir $upload_dir unless -d $upload_dir;

    my ($name, $ext) = ($safe_orig =~ /^(.*?)(\.[^.]*)?$/);
    $name = 'upload' if !defined($name) || $name eq '';
    $ext = '' if !defined($ext);
    my $target_name = $name . '_' . random_hex8() . $ext;
    my $target_path = "$upload_dir/$target_name";

    if (open my $out, '>', $target_path) {
        binmode $out;
        print {$out} $file_bytes;
        close $out;
        my $size = -s $target_path;
        $size = 0 if !defined $size;
        log_message("Saved file: $target_name size=$size");
        redirect_response('/success_upload.html');
        return;
    }

    log_message("Failed writing file: $target_path");
    redirect_response('/failure_upload.html');
}

main();
