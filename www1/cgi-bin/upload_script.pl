#!/usr/bin/env perl
# tells the system to run this file with Perl when it is executed as script
use strict;
use warnings;
# timestamp format
use POSIX qw(strftime); 
use FindBin qw($RealBin);

# Resolve paths relative to this script so the CGI works no matter where it is launched from.
# my - create local variable/ declaration
# $RealBin holds the real directory path of this script
my $script_dir = $RealBin;
my $root_dir = "$script_dir/..";
my $upload_dir = "$root_dir/archive";
my $log_dir = "$root_dir/log";
my $log_file = "$log_dir/upload_script.log";

sub log_message {
    my ($msg) = @_;
    # Best-effort logging: write a timestamped line if the log file is available.
    # eval means the log in written in {} because it wont affect the script running even if log crash
    eval {
        # Create log folder if it does not already exist
        mkdir $log_dir unless -d $log_dir;
        # open file in append mode
        open my $fh, '>>', $log_file or return;
        # Create the timestam
        my $ts = strftime('%Y-%m-%d %H:%M:%S', localtime);
        # write log into it
        print $fh "[$ts] [upload_script.pl] $msg\n";
        # close file
        close $fh;
    };
}

sub redirect_response {
    my ($location) = @_;
    # Send a redirect back to the browser using 303 See Other.
    print "Status: 303 See Other\r\n";
    print "Location: $location\r\n";
    print "Content-Type: text/html\r\n";
    print "Content-Length: 0\r\n\r\n";
}

# Clean the file name remove the path to remain filename
sub sanitize_filename {
    my ($name) = @_;
    # Strip any path components and remove characters that could escape the upload folder.
    # Remove any path before the filename
    $name =~ s/^.*[\\\/]//;
    # remove newline in the name
    $name =~ s/[\r\n]//g;
    # remove / and \
    $name =~ s/[\\\/]/_/g;
    return $name;
}

sub parse_multipart {
    my ($ctype, $body) = @_;
    # ctype is content type
    # body is the full post body form the browser
    # Multipart/form-data uses a boundary string to separate each field.
    
    # If the boundary is missing, the body is not a valid upload payload.
    return (undef, undef) unless $ctype =~ /boundary=([^;]+)/;
    my $boundary = $1;
    
    # Remove optional quotes around the boundary value.
    $boundary =~ s/^"|"$//g;
    # Build the marker that appears between parts in the raw request body.
    my $marker = '--' . $boundary;

    # Split the request body into parts and inspect each part one by one.
    my @parts = split(/\Q$marker\E/, $body);
    foreach my $part (@parts) {
        # Skip the final closing boundary marker.
        next if $part =~ /^--\s*$/;
        # Remove extra newlines that may appear around each part.
        $part =~ s/^\r?\n//;
        $part =~ s/\r?\n$//;
        # Every part should have headers, then a blank line, then the content.
        next unless $part =~ /\r\n\r\n/s;
        my ($headers, $content) = split(/\r\n\r\n/, $part, 2);

        # Only keep the field named file1, which is the upload input in the form.
        next unless $headers =~ /Content-Disposition:\s*form-data;[^\r\n]*name="file1"/i;
        # Extract the original filename sent by the browser.
        next unless $headers =~ /filename="([^"]*)"/i;
        my $filename = $1;
        # Remove trailing newline or boundary leftovers from the file content.
        $content =~ s/\r\n$//;
        $content =~ s/--$//;
        return ($filename, $content);
    }

    return (undef, undef);
}

sub random_hex8 {
    # Generate a short random suffix so uploaded files do not overwrite each other.
    my @h = ('0'..'9', 'a'..'f');
    my $s = '';
    for (1..8) { $s .= $h[int(rand(@h))]; }
    return $s;
}

sub main {
    # Log the start of each upload request.
    log_message('Request started');

    my $method = $ENV{'REQUEST_METHOD'} // '';
    # Accept uploads only through POST.
    if ($method ne 'POST') {
        log_message("Invalid method: $method");
        print "Status: 405 Method Not Allowed\r\nContent-Type: text/plain\r\n\r\nOnly POST requests are allowed";
        return;
    }

    my $ctype = $ENV{'CONTENT_TYPE'} // '';
    # The form must send multipart/form-data for file uploads.
    if ($ctype !~ m{^multipart/form-data}i) {
        log_message("Invalid content type: $ctype");
        print "Status: 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nExpected multipart/form-data";
        return;
    }

    my $len = $ENV{'CONTENT_LENGTH'} // 0;
    $len = 0 if $len !~ /^\d+$/;
    # An empty body means there is no file data to process.
    if ($len <= 0) {
        log_message('Empty request body');
        redirect_response('/failure_upload.html');
        return;
    }

    # Read the request body in chunks so we can handle large uploads safely.
    my $body = '';
    my $remaining = $len;
    while ($remaining > 0) {
        my $chunk = '';
        my $read = read(STDIN, $chunk, $remaining);
        if (!defined $read) {
            log_message('Failed reading request body');
            redirect_response('/failure_upload.html');
            return;
        }
        last if $read == 0;
        $body .= $chunk;
        $remaining -= $read;
    }

    # If the body was truncated, treat the upload as failed.
    if (length($body) != $len) {
        log_message('Incomplete request body');
        redirect_response('/failure_upload.html');
        return;
    }

    # Pull the uploaded filename and raw file bytes out of the multipart body.
    my ($orig, $file_bytes) = parse_multipart($ctype, $body);
    if (!defined $orig || !defined $file_bytes || $orig eq '') {
        log_message('No file1 field in multipart payload');
        redirect_response('/failure_upload.html');
        return;
    }

    # Clean the filename before using it on disk.
    my $safe_orig = sanitize_filename($orig);
    if ($safe_orig eq '') {
        log_message('Filename sanitization failed');
        redirect_response('/failure_upload.html');
        return;
    }

    # Make sure the upload directory exists before writing the file.
    mkdir $upload_dir unless -d $upload_dir;

    # Keep the original extension but add a random suffix to avoid collisions.
    my ($name, $ext) = ($safe_orig =~ /^(.*?)(\.[^.]*)?$/);
    $name = 'upload' if !defined($name) || $name eq '';
    $ext = '' if !defined($ext);
    my $target_name = $name . '_' . random_hex8() . $ext;
    my $target_path = "$upload_dir/$target_name";

    # Write the uploaded bytes to disk and redirect to the success page.
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

# Run the main request handler when CGI starts.
main();
