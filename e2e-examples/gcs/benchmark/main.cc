#include <grpc/status.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>
#include <stdio.h>

#include <fstream>
#include <streambuf>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/time/time.h"
#include "google/storage/v1/storage.grpc.pb.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

ABSL_FLAG(string, access_token, "", "Access token for auth");
ABSL_FLAG(string, host, "dns:///storage.googleapis.com:443", "Host to reach");
ABSL_FLAG(string, bucket, "gcs-grpc-team-veblush1",
          "Bucket to fetch object from");
ABSL_FLAG(string, object, "1G.txt", "Object to download");
ABSL_FLAG(int, runs, 1, "Number of times to run the download");
ABSL_FLAG(int, warmup_runs, 1, "Number of times to run the download");
ABSL_FLAG(bool, directpath, true, "Whether to allow DirectPath");
ABSL_FLAG(bool, verbose, false, "Show debug output and progress updates");

std::shared_ptr<grpc::Channel> CreateOAuthChannel() {
  std::string access_token = absl::GetFlag(FLAGS_access_token);
  if (access_token.empty()) {
    grpc::ChannelArguments channel_args;
    channel_args.SetServiceConfigJSON(
        "{\"loadBalancingConfig\":[{\"grpclb\":{"
        "\"childPolicy\":[{\"pick_first\":{}}]}}]"
        "}");
    if (!absl::GetFlag(FLAGS_directpath)) {
      channel_args.SetInt("grpc.dns_enable_srv_queries",
                          0);  // Disable DirectPath
    }
    std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(
        absl::GetFlag(FLAGS_host), grpc::GoogleDefaultCredentials(),
        channel_args);
    return channel;
  } else {
    std::shared_ptr<grpc::CallCredentials> call_credentials =
        grpc::AccessTokenCredentials(access_token);
    std::shared_ptr<grpc::ChannelCredentials> channel_credentials =
        grpc::SslCredentials(grpc::SslCredentialsOptions());
    std::shared_ptr<grpc::ChannelCredentials> credentials =
        grpc::CompositeChannelCredentials(channel_credentials,
                                          call_credentials);
    std::shared_ptr<grpc::Channel> channel =
        grpc::CreateChannel(absl::GetFlag(FLAGS_host), credentials);
    return channel;
  }
}

int main(int argc, char **argv) {
  absl::ParseCommandLine(argc, argv);

  bool debug = absl::GetFlag(FLAGS_verbose);

  std::shared_ptr<grpc::Channel> channel;
  channel = CreateOAuthChannel();

  auto objectsStub = google::storage::v1::Storage::NewStub(channel);

  google::storage::v1::GetObjectMediaRequest request;
  request.set_bucket(absl::GetFlag(FLAGS_bucket));
  request.set_object(absl::GetFlag(FLAGS_object));

  google::storage::v1::GetObjectMediaResponse response;

  long ttfb_milli_counter = 0;
  long long transfer_time_milli_counter = 0;

  unsigned long bytesTransferred = 0;
  int num_runs = absl::GetFlag(FLAGS_runs);
  std::vector<int> run_time_millis;
  for (int run = 0; run < num_runs + absl::GetFlag(FLAGS_warmup_runs); run++) {
    absl::Time run_start = absl::Now();
    grpc::ClientContext context;
    std::unique_ptr<
        ::grpc::ClientReader<::google::storage::v1::GetObjectMediaResponse>>
        reader = objectsStub->GetObjectMedia(&context, request);

    bool gotFirstByte = false;
    absl::Time first_byte;

    bytesTransferred = 0;

    while (reader->Read(&response)) {
      if (!gotFirstByte) {
        gotFirstByte = true;
        first_byte = absl::Now();
      }
      bytesTransferred += response.checksummed_data().content().size();
    }
    auto status = reader->Finish();
    absl::Time run_end = absl::Now();

    if (!status.ok()) {
      cerr << "Download Failure!" << endl;
      cerr << status.error_message() << endl;
      return 1;
    }

    absl::Duration ttfb = first_byte - run_start;
    absl::Duration runtime = run_end - run_start;

    bool used_directpath = context.peer().rfind("ipv6:[2001:") == 0;
    if (used_directpath ^ absl::GetFlag(FLAGS_directpath)) {
      cerr << "DirectPath flag is " << absl::GetFlag(FLAGS_directpath)
           << ", but using directpath is " << used_directpath << endl;
      cerr << "Peer was " << context.peer() << endl;
      return 1;
    }

    if (debug) {
      cout << "Was connected to " << context.peer();
      cout << "Downloaded " << bytesTransferred << " bytes." << endl;
      cout << "Time to first byte: " << absl::ToInt64Milliseconds(ttfb)
           << " millis." << endl;
      cout << "Total transfer time: " << absl::ToDoubleSeconds(runtime)
           << " seconds." << endl;
      cout << "Download speed of "
           << (bytesTransferred / (absl::ToDoubleSeconds(runtime) * 1048576))
           << " MBps." << endl
           << endl;
    }

    if (run >= absl::GetFlag(FLAGS_warmup_runs)) {
      run_time_millis.push_back(absl::ToInt64Milliseconds(runtime));
    }

    ttfb_milli_counter += absl::ToInt64Milliseconds(ttfb);
    transfer_time_milli_counter += absl::ToInt64Milliseconds(runtime);
  }

  if (absl::GetFlag(FLAGS_runs) != 1) {
    std::sort(run_time_millis.begin(), run_time_millis.end());
    int run_time_sum = 0;
    for (std::size_t i = 0; i < run_time_millis.size(); i++) {
      run_time_sum += run_time_millis.at(i);
    }
    cout << "\n\t\tAvg\tMin\tp50\tp90\tp99\tMax\n"
         << "Time(ms)\t" << run_time_sum / num_runs << "\t"
         << run_time_millis.at(0) << "\t" << run_time_millis.at(num_runs / 2)
         << "\t" << run_time_millis.at((int)(num_runs * 0.9)) << "\t"
         << run_time_millis.at((int)(num_runs * 0.99)) << "\t"
         << run_time_millis.at(num_runs - 1) << "\n";
  }

  return 0;
}
