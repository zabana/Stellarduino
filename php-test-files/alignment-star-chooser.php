<?php

header("Content-type: text/plain; charset=utf-8");

// define constants

$k = 1.002737908; // solar day (24h00m00s) / sidereal day (23h56m04.0916s)

$lat = dms2rad(27,26,20,'s');
$long = dms2rad(153,2,2,'e');

// Time of observation
$time = new DateTime('now');

$stars = array();
$file = fopen('stars.csv','r');

// load alignment stars
while($csv = fgetcsv($file)) {
    $stars[] = array(
        'name' => $csv[0],
        'mag' => $csv[1],
        'ra' => $csv[2],
        'dec' => $csv[3]
    );
}
fclose($file);

// calculate locat sidereal time
$lst = getLST($time, $long);


// echo test data
echo "Lat: ".rad2dms($lat)."\n";
echo "Long: ".rad2dms($long)."\n";
echo "Long: ".rad2hms($long)."\n";
echo "LST: ".rad2hms($lst)."\n";


$counter = 0;
// calculate alt az of each alignment star
foreach($stars as &$star) {
    $counter++;

    $star['ha'] = $lst - $star['ra'];
    if ($star['ha'] < 0) $star['ha'] += 2*M_PI;

    $star['alt'] = asin(sin($star['dec']) * sin($lat) + cos($star['dec']) * cos($lat) * cos($star['ha']));
    $star['az'] = acos((sin($star['dec']) - sin($star['alt']) * sin($lat)) / (cos($star['alt']) * cos($lat)));
    
    // show all stars with alts above zero
    if ($star['alt'] > 0) {
        echo $star['name'].":\n";
        echo "   ra: ".rad2hms($star['ra'])."\n";
        echo "   dec: ".rad2dms($star['dec'])."\n";
//        echo "   ha: ".rad2hms($star['ha'])."\n";
        echo "   alt: ".rad2dms($star['alt'])."\n";
        echo "   az: ".rad2dms($star['az'])."\n";
    }
    
//    if ($counter >= 6) break;
}

// convenience functions
function hms2rad($h, $m, $s, $hemisphere) {
    return deg2rad((($h + $m / 60 + $s / 3600) * (($hemisphere == 's' || $hemisphere == 'w') ? -1 : 1)) * 15);
}

function dms2rad($d, $m, $s, $hemisphere) {
    return deg2rad(($d + $m / 60 + $s / 3600) * (($hemisphere == 's' || $hemisphere == 'w') ? -1 : 1));
}

function hms2deg($h, $m, $s, $hemisphere) {
    $decdeg = ($h + $m / 60 + $s / 3600) * 15;
    
    $degrees = floor($decdeg);
    $minutes = floor(($decdeg - $degrees) * 60);
    $seconds = ($decdeg - $degrees - $minutes / 60) * 3600;
    
    return (($hemisphere == 's' || $hemisphere == 'e') ? '-' : '').$degrees."d".$minutes."m".$seconds."s";
}

function rad2dms($rad) {
    $decdeg = rad2deg($rad);
    $degrees = floor(abs($decdeg));
    $minutes = floor((abs($decdeg) - $degrees) * 60);
    $seconds = round((abs($decdeg) - $degrees - $minutes / 60.0) * 3600);
    return ($rad < 0 ? '-' : '').$degrees."°".str_pad($minutes,2,'0',STR_PAD_LEFT)."'".str_pad($seconds,2,'0',STR_PAD_LEFT)."\"";
}

function rad2hms($rad) {
    $dechour = $rad * 12.0 / M_PI;
    $hours = floor(abs($dechour));
    $minutes = floor((abs($dechour) - $hours) * 60);
    $seconds = round((abs($dechour) - $hours - $minutes / 60.0) * 3600);
    return ($rad < 0 ? '-' : '').str_pad($hours,2,'0',STR_PAD_LEFT).":".str_pad($minutes,2,'0',STR_PAD_LEFT)."'".str_pad($seconds,2,'0',STR_PAD_LEFT)."\"";
}


// don't use for dates before 1582 AD - lolwtf
function getLST($time, $long) {
    $gmt = $time->setTimezone(new DateTimeZone('GMT'));
    
    $year = (integer)$gmt->format('Y');
    $month = (integer)$gmt->format('n');
    $day = (integer)$gmt->format('j');
    $hour =  $gmt->format('G') + $gmt->format('i') / 60.0 + $gmt->format('s') / 3600.0;

    if ((integer)$gmt->format('n') < 3) {
        $year = $year - 1;
        $month = $month + 12;
    }
    $gr = 2 - floor($year / 100.0) + floor(floor($year / 100.0) / 4.0);

    $jd = floor(365.25 * $year) + floor(30.6001 * ($month + 1)) + $day + 1720994.5 + $gr;
    $jd2 = $jd + $hour / 24.0;

    $time = ($jd - 2415020) / 36525.0;
    $ss = 6.6460656 + 2400.051 * $time + 0.00002581 * ($time ^ 2);
    $st = ($ss / 24.0 - floor($ss / 24.0)) * 24;

    $sa = $st + $hour * 1.002737908;
    $sa = $sa + $long / (M_PI / 12.0);
    
    if ($sa < 0) $sa = $sa + 24;
    if ($sa > 24) $sa = $sa - 24;
    
    return $sa / 12.0 * M_PI;
}